#include "audiosystem.h"
#include "../../src/gloomapp.h"
#include "gloom/render/icamera.h"
#include <de/Log>
#include <de/Folder>
#include <de/ByteArrayFile>
#include <fmod.hpp>
#include <fmod_errors.h>
#include <QHash>
#include <QSet>

using namespace de;

namespace gloom {

/**
 * Adapter that allows FMOD to read files via Doomsday.
 */
struct FileAdapter
{
    ByteArrayFile const *file; // random access needed
    dsize pos;

    FileAdapter(ByteArrayFile const &f) : file(&f), pos(0) {}

    static FMOD_RESULT F_CALLBACK open(const char *  name,
                                       unsigned int *filesize,
                                       void **       handle,
                                       void *)
    {
        try
        {
            String fileName = String::fromUtf8(name);
            FileAdapter *adapter = new FileAdapter(App::rootFolder().locate<ByteArrayFile const>(fileName));
            *filesize = uint(adapter->file->size());
            *handle = adapter;
        }
        catch(Error const &er)
        {
            LOG_AS("FileAdapter::open");
            LOGDEV_RES_WARNING("") << er.asText();
            return FMOD_ERR_FILE_NOTFOUND;
        }
        return FMOD_OK;
    }

    static FMOD_RESULT F_CALLBACK close(void *handle, void *)
    {
        delete reinterpret_cast<FileAdapter *>(handle);
        return FMOD_OK;
    }

    static FMOD_RESULT F_CALLBACK
                       read(void *handle, void *buffer, unsigned int sizebytes, unsigned int *bytesread, void *)
    {
        FMOD_RESULT result = FMOD_OK;
        try
        {
            FileAdapter *adapter = reinterpret_cast<FileAdapter *>(handle);
            dsize count = min(dsize(sizebytes), adapter->file->size() - adapter->pos);
            if(count < sizebytes)
            {
                result = FMOD_ERR_FILE_EOF;
            }
            adapter->file->get(adapter->pos, reinterpret_cast<IByteArray::Byte *>(buffer), count);
            //qDebug() << "read" << count << "bytes at" << adapter->pos;
            adapter->pos += count;
            *bytesread = uint(count);
        }
        catch(Error const &er)
        {
            LOG_AS("FileAdapter::read");
            LOGDEV_RES_WARNING("") << er.asText();
            return FMOD_ERR_FILE_BAD;
        }
        return result;
    }

    static FMOD_RESULT F_CALLBACK seek(void *handle, unsigned int pos, void *)
    {
        //qDebug() << "seek" << pos;
        FileAdapter *adapter = reinterpret_cast<FileAdapter *>(handle);
        adapter->pos = pos;
        return FMOD_OK;
    }
};

static AudioSystem *theAudioSystem = {nullptr};

DE_PIMPL(AudioSystem)
{
    struct AudibleSound;

    /**
     * Audio waveform passed onto the FMOD. A separate variant is prepared for looping
     * and non-looping sounds, as they might have to be set up differently by FMOD.
     */
    class CachedWaveform
    {
        // Created on demand:
        FMOD::Sound *_sound       = nullptr;
        FMOD::Sound *_loopSound   = nullptr;
        FMOD::Sound *_sound3D     = nullptr;
        FMOD::Sound *_loopSound3D = nullptr;

    public:
        FMOD::System *  system = nullptr;
        Waveform const &wf;

        typedef QSet<AudibleSound *> PlayingSounds; // owned
        PlayingSounds sounds;

        /**
         * Construct a cached waveform.
         * @param sys       FMOD System.
         * @param waveform  Waveform data. Does @em NOT take ownership or copy of this
         *                  object. The original waveform must exist elsewhere.
         */
        CachedWaveform(FMOD::System *sys, Waveform const &waveform)
            : system(sys)
            , wf(waveform)
        {}

        ~CachedWaveform()
        {
            PlayingSounds s = sounds;
            qDeleteAll(s); // sounds will unregister themselves
            if(_sound)       { _sound->release();       _sound = 0; }
            if(_loopSound)   { _loopSound->release();   _loopSound = 0; }
            if(_sound3D)     { _sound3D->release();     _sound3D = 0; }
            if(_loopSound3D) { _loopSound3D->release(); _loopSound3D = 0; }
        }

        enum Flag
        {
            NoLoop = 0,
            Loop   = 0x1,
            Pos3D  = 0x2
        };

        FMOD::Sound *create(int flags)
        {
            FMOD_CREATESOUNDEXINFO info;
            zap(info);
            info.cbsize = sizeof(info);

            int commonFlags = //FMOD_SOFTWARE |
                    (flags & Loop  ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) |
                    (flags & Pos3D ? FMOD_3D : FMOD_2D);

            FMOD::Sound *sound = 0;
            if(wf.format() == audio::PCMLittleEndian)
            {
                info.length           = uint(wf.sampleData().size());
                info.defaultfrequency = wf.sampleRate();
                info.numchannels      = wf.channelCount();
                info.format           = (wf.bitsPerSample() == 8?  FMOD_SOUND_FORMAT_PCM8 :
                                         wf.bitsPerSample() == 16? FMOD_SOUND_FORMAT_PCM16 :
                                         wf.bitsPerSample() == 24? FMOD_SOUND_FORMAT_PCM24 :
                                                                   FMOD_SOUND_FORMAT_PCM32);
                system->createSound(wf.sampleData().c_str(),
                                    FMOD_OPENRAW | FMOD_OPENMEMORY_POINT | commonFlags,
                                    &info, &sound);
            }
            else
            {
                // Instruct FMOD to load it from the source file.
                DE_ASSERT(wf.sourceFile() != 0);

                info.fileuseropen  = FileAdapter::open;
                info.fileuserclose = FileAdapter::close;
                info.fileuserread  = FileAdapter::read;
                info.fileuserseek  = FileAdapter::seek;

                // Compressed sound formats might be understood by FMOD.
                FMOD_RESULT result = system->createSound(
                    wf.sourceFile()->path().c_str(),
                    /*FMOD_UNICODE |*/ FMOD_CREATECOMPRESSEDSAMPLE | commonFlags,
                    &info,
                    &sound);
                if(result != FMOD_OK)
                {
                    LOG_AUDIO_WARNING("Failed to load %s: %s")
                            << wf.sourceFile()->description() << FMOD_ErrorString(result);
                }
            }
            return sound;
        }

        FMOD::Sound *getSound(int flags)
        {
            FMOD::Sound *ptr = 0;
            if(flags & Pos3D)
            {
                ptr = (flags & Loop? _loopSound3D : _sound3D);
            }
            else
            {
                ptr = (flags & Loop? _loopSound : _sound);
            }
            if (!ptr) ptr = create(flags);
            return ptr;
        }
    };

    static FMOD_RESULT F_CALLBACK channelCallback(FMOD_CHANNELCONTROL * channelcontrol,
                                                  FMOD_CHANNELCONTROL_TYPE          controltype,
                                                  FMOD_CHANNELCONTROL_CALLBACK_TYPE callbacktype,
                                                  void *,
                                                  void *)
    {
        if (controltype != FMOD_CHANNELCONTROL_CHANNEL)
        {
            return FMOD_OK;
        }
        FMOD::Channel *channel = reinterpret_cast<FMOD::Channel *>(channelcontrol);
        switch (callbacktype)
        {
        case FMOD_CHANNELCONTROL_CALLBACK_END: {
            // Playback has finished on the channel.
            Sound *sound = 0;
            channel->getUserData(reinterpret_cast<void **>(&sound));
            if(sound)
            {
                sound->stop();
            }
            break; }

        default:
            break;
        }
        return FMOD_OK;
    }

    /**
     * Sound that is possibly playing on an FMOD channel. Implements de::Sound that
     * allows third parties to control the sound and query its status.
     */
    struct AudibleSound : public Sound
    {
        CachedWaveform &cached;
        FMOD::Channel *channel;
        PlayingMode _mode;
        float _originalFreq;

        AudibleSound(CachedWaveform &cachedWaveform)
            : cached(cachedWaveform)
            , channel(0)
            , _mode(NotPlaying)
        {
            cachedWaveform.sounds.insert(this);
        }

        ~AudibleSound()
        {
            if(_mode == Once) _mode = OnceDontDelete; // deleting now

            stop();

            DE_FOR_AUDIENCE2(Deletion, i) i->soundBeingDeleted(*this);
            cached.sounds.remove(this);
        }

        /// Start the sound on a new channel but leave it paused.
        void alloc()
        {
            DE_ASSERT(!channel);

            int flags = 0;
            if (_mode == Looping) flags |= CachedWaveform::Loop;
            if (positioning() != Stereo) flags |= CachedWaveform::Pos3D;

            cached.system->playSound(//FMOD_CHANNEL_FREE,
                                     cached.getSound(flags),
                                     NULL,
                                     true /*paused*/,
                                     &channel);
            //qDebug() << "sound started" << (cached.wf.sourceFile()? cached.wf.sourceFile()->path() : "") << channel;
            if (channel)
            {
                channel->setUserData(this); // corresponding Sound instance
                channel->setCallback(channelCallback);

                _originalFreq = cached.wf.sampleRate();
                if (_originalFreq == 0.f)
                {
                    // Ask FMOD about the frequency.
                    float freq;
                    channel->getFrequency(&freq);
                    _originalFreq = freq;
                }
            }
        }

        void release()
        {
            _mode = NotPlaying;
            if (channel)
            {
                channel->setUserData(NULL);
                channel->setCallback(NULL);
                channel->stop();
                channel = 0;
            }
        }

        void notifyStop()
        {
            DE_FOR_AUDIENCE2(Stop, i) i->soundStopped(*this);
        }

        void play(PlayingMode playMode)
        {
            if (isPlaying()) return;

            _mode = playMode;
            alloc();
            update();
            channel->setPaused(false);

            DE_FOR_AUDIENCE2(Play, i) { i->soundPlayed(*this); }
        }

        void stop()
        {
            if(_mode == NotPlaying) return;

            //qDebug() << "sound stopped" << channel;

            notifyStop();

            if(_mode == Once)
            {
                release();
                delete this;
                return;
            }

            release();
        }

        void pause()
        {
            if(channel)
            {
                channel->setPaused(true);
            }
        }

        void resume()
        {
            if(channel)
            {
                channel->setPaused(false);
            }
        }

        void update()
        {
            if(!channel) return;

            // Apply current properties to the channel.
            channel->setVolume(volume());
            channel->setPan(pan());
            channel->setFrequency(_originalFreq * frequency());

            if(positioning() != Stereo)
            {
                FMOD_VECTOR pos, vel;

                pos.x = position().x;
                pos.y = position().y;
                pos.z = position().z;

                vel.x = velocity().x;
                vel.y = velocity().y;
                vel.z = velocity().z;

                channel->set3DAttributes(&pos, &vel);
                channel->set3DMinMaxDistance(minDistance(), 10000);
                channel->set3DSpread(spatialSpread());
            }
        }

        PlayingMode mode() const
        {
            return _mode;
        }

        bool isPaused() const
        {
            if(!channel) return false;

            bool paused = false;
            channel->getPaused(&paused);
            return paused;
        }
    };

    FMOD::System *system;
    typedef QHash<Waveform const *, CachedWaveform *> Cache;
    Cache cache;
    const ICamera *listenerCamera = nullptr;

    Impl(Public *i)
        : Base(i)
        , system(0)
    {
        init();
        theAudioSystem = i;
    }

    ~Impl()
    {
        deinit();
        theAudioSystem = nullptr;
    }

    void init()
    {
        // Initialize FMOD.
        FMOD_RESULT result = FMOD::System_Create(&system);
        if (result != FMOD_OK)
        {
            throw NativeError("AudioSystem::init", FMOD_ErrorString(result));
        }

        // Configure FMOD appropriately.
        //system->setSpeakerMode(FMOD_SPEAKERMODE_STEREO);

        result = system->init(100, FMOD_INIT_NORMAL, 0);
        if (result != FMOD_OK)
        {
            throw NativeError("AudioSystem::init", FMOD_ErrorString(result));
        }

        LOG_AUDIO_NOTE("FMOD Sound System © Firelight Technologies Pty, Ltd., 1994-2014");

        int numPlugins = 0;
        system->getNumPlugins(FMOD_PLUGINTYPE_CODEC, &numPlugins);
        LOG_AUDIO_VERBOSE("FMOD codecs:");
        for (int i = 0; i < numPlugins; ++i)
        {
            duint handle;
            system->getPluginHandle(FMOD_PLUGINTYPE_CODEC, i, &handle);

            Block name(100);
            duint version = 0;
            system->getPluginInfo(
                handle, 0, reinterpret_cast<char *>(name.data()), int(name.size()), &version);
            LOG_AUDIO_VERBOSE(" - %i: %s v%x") << i << String::fromLatin1(name) << version;
        }
    }

    void deinit()
    {
        // Free all loaded sounds.
        qDeleteAll(cache.values());
        cache.clear();

        system->release();
        system = 0;
    }

    void updateListener()
    {
        if (const ICamera *cam = listenerCamera)
        {
            const Vec3f camPos = cam->cameraPosition();

            const FMOD_VECTOR pos = {camPos.x, camPos.y, camPos.z};
            const FMOD_VECTOR fwd = {cam->cameraFront().x, cam->cameraFront().y, cam->cameraFront().z};
            const FMOD_VECTOR up  = {cam->cameraUp().x,    cam->cameraUp().y,    cam->cameraUp().z};

            system->set3DListenerAttributes(0, &pos, 0, &fwd, &up);
        }
    }

    void refresh()
    {
        updateListener();
        system->update();
    }

    Sound &load(Waveform const &waveform)
    {
        DE_ASSERT(system != 0);

        Cache::iterator found  = cache.find(&waveform);
        CachedWaveform *cached = 0;
        if (found == cache.end())
        {
            // Load it now.
            cache.insert(&waveform, cached = new CachedWaveform(system, waveform));
        }
        else
        {
            // Use existing.
            cached = *found;
        }
        return *new AudibleSound(*cached);
    }
};

AudioSystem::AudioSystem() : d(new Impl(this))
{}

AudioSystem &AudioSystem::get()
{
    return *theAudioSystem;
}

Sound &AudioSystem::newSound(Waveform const &waveform)
{
    return d->load(waveform);
}

Sound &AudioSystem::newSound(DotPath const &appWaveform)
{
    Sound &sound = newSound(GloomApp::waveforms().waveform(appWaveform));
    DE_FOR_AUDIENCE(NewSound, i)
    {
        i->newSoundCreated(sound, appWaveform);
    }
    return sound;
}

void AudioSystem::timeChanged(Clock const &)
{
    d->refresh();
}

void AudioSystem::setListener(const ICamera *camera)
{
    d->listenerCamera = camera;
}

const ICamera *AudioSystem::listener() const
{
    return d->listenerCamera;
}

} // namespace gloom
