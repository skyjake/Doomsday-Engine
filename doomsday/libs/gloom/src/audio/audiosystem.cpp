#include "gloom/audio/audiosystem.h"
#include "gloom/render/icamera.h"

#include <de/BaseGuiApp>
#include <de/ByteArrayFile>
#include <de/Folder>
#include <de/Hash>
#include <de/Log>
#include <de/Set>

#if defined (LIBGLOOM_HAVE_FMOD)
#  include <fmod.h>
#  include <fmod_errors.h>
#endif

using namespace de;

namespace gloom {

#if defined (LIBGLOOM_HAVE_FMOD)
/**
 * Adapter that allows FMOD to read files via Doomsday.
 */
struct FileAdapter
{
    const ByteArrayFile *file; // random access needed
    dsize pos;

    FileAdapter(const ByteArrayFile &f) : file(&f), pos(0) {}

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
        catch(const Error &er)
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
        catch(const Error &er)
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
#endif

static AudioSystem *theAudioSystem = {nullptr};

DE_PIMPL(AudioSystem)
{
#if defined (LIBGLOOM_HAVE_FMOD)

    struct AudibleSound;

    /**
     * Audio waveform passed onto the FMOD. A separate variant is prepared for looping
     * and non-looping sounds, as they might have to be set up differently by FMOD.
     */
    class CachedWaveform
    {
        // Created on demand:
        FMOD_SOUND *_sound       = nullptr;
        FMOD_SOUND *_loopSound   = nullptr;
        FMOD_SOUND *_sound3D     = nullptr;
        FMOD_SOUND *_loopSound3D = nullptr;

    public:
        FMOD_SYSTEM *  system = nullptr;
        const Waveform &wf;

        typedef Set<AudibleSound *> PlayingSounds; // owned
        PlayingSounds sounds;

        /**
         * Construct a cached waveform.
         * @param sys       FMOD System.
         * @param waveform  Waveform data. Does @em NOT take ownership or copy of this
         *                  object. The original waveform must exist elsewhere.
         */
        CachedWaveform(FMOD_SYSTEM *sys, const Waveform &waveform)
            : system(sys)
            , wf(waveform)
        {}

        ~CachedWaveform()
        {
            PlayingSounds s = sounds;
            deleteAll(s); // sounds will unregister themselves
            if(_sound)       { FMOD_Sound_Release(_sound);       _sound = 0; }
            if(_loopSound)   { FMOD_Sound_Release(_loopSound);   _loopSound = 0; }
            if(_sound3D)     { FMOD_Sound_Release(_sound3D);     _sound3D = 0; }
            if(_loopSound3D) { FMOD_Sound_Release(_loopSound3D); _loopSound3D = 0; }
        }

        enum Flag
        {
            NoLoop = 0,
            Loop   = 0x1,
            Pos3D  = 0x2
        };

        FMOD_SOUND *create(int flags)
        {
            FMOD_CREATESOUNDEXINFO info;
            zap(info);
            info.cbsize = sizeof(info);

            int commonFlags = //FMOD_SOFTWARE |
                    (flags & Loop  ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) |
                    (flags & Pos3D ? FMOD_3D : FMOD_2D);

            FMOD_SOUND *sound = 0;
            if(wf.format() == audio::PCMLittleEndian)
            {
                info.length           = uint(wf.sampleData().size());
                info.defaultfrequency = wf.sampleRate();
                info.numchannels      = wf.channelCount();
                info.format           = (wf.bitsPerSample() == 8?  FMOD_SOUND_FORMAT_PCM8 :
                                         wf.bitsPerSample() == 16? FMOD_SOUND_FORMAT_PCM16 :
                                         wf.bitsPerSample() == 24? FMOD_SOUND_FORMAT_PCM24 :
                                                                   FMOD_SOUND_FORMAT_PCM32);
                FMOD_System_CreateSound(system, wf.sampleData().c_str(),
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
                FMOD_RESULT result = FMOD_System_CreateSound(system, 
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

        FMOD_SOUND *getSound(int flags)
        {
            FMOD_SOUND *ptr = 0;
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
        FMOD_CHANNEL *channel = reinterpret_cast<FMOD_CHANNEL *>(channelcontrol);
        switch (callbacktype)
        {
        case FMOD_CHANNELCONTROL_CALLBACK_END: {
            // Playback has finished on the channel.
            Sound *sound = 0;
            FMOD_Channel_GetUserData(channel, reinterpret_cast<void **>(&sound));
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
        FMOD_CHANNEL *channel;
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

            DE_NOTIFY(Deletion, i) i->soundBeingDeleted(*this);
            cached.sounds.remove(this);
        }

        /// Start the sound on a new channel but leave it paused.
        void alloc()
        {
            DE_ASSERT(!channel);

            int flags = 0;
            if (_mode == Looping) flags |= CachedWaveform::Loop;
            if (positioning() != Stereo) flags |= CachedWaveform::Pos3D;

            FMOD_System_PlaySound(cached.system, //FMOD_Channel_FREE,
                                  cached.getSound(flags),
                                  NULL,
                                  true /*paused*/,
                                  &channel);
            //qDebug() << "sound started" << (cached.wf.sourceFile()? cached.wf.sourceFile()->path() : "") << channel;
            if (channel)
            {
                FMOD_Channel_SetUserData(channel, this); // corresponding Sound instance
                FMOD_Channel_SetCallback(channel, channelCallback);

                _originalFreq = cached.wf.sampleRate();
                if (_originalFreq == 0.f)
                {
                    // Ask FMOD about the frequency.
                    float freq;
                    FMOD_Channel_GetFrequency(channel, &freq);
                    _originalFreq = freq;
                }
            }
        }

        void release()
        {
            _mode = NotPlaying;
            if (channel)
            {
                FMOD_Channel_SetUserData(channel, NULL);
                FMOD_Channel_SetCallback(channel, NULL);
                FMOD_Channel_Stop(channel);
                channel = 0;
            }
        }

        void notifyStop()
        {
            DE_NOTIFY(Stop, i) i->soundStopped(*this);
        }

        void play(PlayingMode playMode)
        {
            if (isPlaying()) return;

            _mode = playMode;
            alloc();
            update();
            FMOD_Channel_SetPaused(channel, 0);

            DE_NOTIFY(Play, i) { i->soundPlayed(*this); }
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
                FMOD_Channel_SetPaused(channel, true);
            }
        }

        void resume()
        {
            if(channel)
            {
                FMOD_Channel_SetPaused(channel, false);
            }
        }

        void update()
        {
            if(!channel) return;

            // Apply current properties to the channel.
            FMOD_Channel_SetVolume(channel, volume());
            FMOD_Channel_SetPan(channel, pan());
            FMOD_Channel_SetFrequency(channel, _originalFreq * frequency());

            if(positioning() != Stereo)
            {
                FMOD_VECTOR pos, vel;

                pos.x = position().x;
                pos.y = position().y;
                pos.z = position().z;

                vel.x = velocity().x;
                vel.y = velocity().y;
                vel.z = velocity().z;

                FMOD_Channel_Set3DAttributes(channel, &pos, &vel, NULL);
                FMOD_Channel_Set3DMinMaxDistance(channel, minDistance(), 10000);
                FMOD_Channel_Set3DSpread(channel, spatialSpread());
            }
        }

        PlayingMode mode() const
        {
            return _mode;
        }

        bool isPaused() const
        {
            if(!channel) return false;

            FMOD_BOOL paused = 0;
            FMOD_Channel_GetPaused(channel, &paused);
            return paused != 0;
        }
    };

    FMOD_SYSTEM *system;
    typedef Hash<const Waveform *, CachedWaveform *> Cache;
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
        FMOD_RESULT result = FMOD_System_Create(&system);
        if (result != FMOD_OK)
        {
            throw NativeError("AudioSystem::init", FMOD_ErrorString(result));
        }

        // Configure FMOD appropriately.
        //system->setSpeakerMode(FMOD_SPEAKERMODE_STEREO);

        result = FMOD_System_Init(system, 100, FMOD_INIT_NORMAL, 0);
        if (result != FMOD_OK)
        {
            throw NativeError("AudioSystem::init", FMOD_ErrorString(result));
        }

        LOG_AUDIO_NOTE("FMOD Sound System © Firelight Technologies Pty, Ltd., 1994-2014");

        int numPlugins = 0;
        FMOD_System_GetNumPlugins(system, FMOD_PLUGINTYPE_CODEC, &numPlugins);
        LOG_AUDIO_VERBOSE("FMOD codecs:");
        for (int i = 0; i < numPlugins; ++i)
        {
            duint handle;
            FMOD_System_GetPluginHandle(system, FMOD_PLUGINTYPE_CODEC, i, &handle);

            Block name(100);
            duint version = 0;
            FMOD_System_GetPluginInfo(system,
                handle, 0, reinterpret_cast<char *>(name.data()), int(name.size()), &version);
            LOG_AUDIO_VERBOSE(" - %i: %s v%x") << i << String::fromLatin1(name) << version;
        }
    }

    void deinit()
    {
        // Free all loaded sounds.
        cache.deleteAll();
        cache.clear();

        FMOD_System_Release(system);
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

            FMOD_System_Set3DListenerAttributes(system, 0, &pos, 0, &fwd, &up);
        }
    }

    void refresh()
    {
        updateListener();
        FMOD_System_Update(system);
    }

    Sound &load(const Waveform &waveform)
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
            cached = found->second;
        }
        return *new AudibleSound(*cached);
    }
    
#else
    // Dummy implementation.
    struct DummySound : public de::Sound
    {
        void play(PlayingMode) {}
        void stop() {}
        void pause() {}
        void resume() {}
        PlayingMode mode() const { return NotPlaying; }
        bool isPaused() const { return true; }
        void update() {}
    };
    DummySound dummy;
    const ICamera *listenerCamera = nullptr;
    
    Impl(Public *i) : Base(i)
    {
        theAudioSystem = i;
    }
    
    ~Impl()
    {
        theAudioSystem = nullptr;
    }
    
    void refresh() {}
    
    Sound &load(const Waveform &)
    {
        return dummy;
    }
       
#endif
};

AudioSystem::AudioSystem() : d(new Impl(this))
{}

AudioSystem &AudioSystem::get()
{
    return *theAudioSystem;
}

bool AudioSystem::isAvailable()
{
    return theAudioSystem != nullptr;
}

Sound &AudioSystem::newSound(const Waveform &waveform)
{
    return d->load(waveform);
}

Sound &AudioSystem::newSound(const DotPath &appWaveform)
{
    Sound &sound = newSound(BaseGuiApp::waveforms().waveform(appWaveform));
    DE_NOTIFY_VAR(NewSound, i)
    {
        i->newSoundCreated(sound, appWaveform);
    }
    return sound;
}

void AudioSystem::timeChanged(const Clock &)
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
