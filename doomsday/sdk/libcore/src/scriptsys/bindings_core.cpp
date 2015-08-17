/** @file bindings_core.cpp
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/Animation"
#include "de/App"
#include "de/BlockValue"
#include "de/Context"
#include "de/DictionaryValue"
#include "de/File"
#include "de/Function"
#include "de/NativeValue"
#include "de/Path"
#include "de/Record"
#include "de/RecordValue"
#include "de/TextValue"

namespace de {

static Value *Function_String_FileNamePath(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.instanceScope().asText().fileNamePath());
}

static Value *Function_String_FileNameExtension(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.instanceScope().asText().fileNameExtension());
}

static Value *Function_String_FileNameWithoutExtension(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.instanceScope().asText().fileNameWithoutExtension());
}

static Value *Function_String_FileNameAndPathWithoutExtension(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.instanceScope().asText().fileNameAndPathWithoutExtension());
}

static Value *Function_String_Upper(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.instanceScope().asText().upper());
}

static Value *Function_String_Lower(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.instanceScope().asText().lower());
}

static Value *Function_Dictionary_Keys(Context &ctx, Function::ArgumentValues const &)
{
    return ctx.instanceScope().as<DictionaryValue>().contentsAsArray(DictionaryValue::Keys);
}

static Value *Function_Dictionary_Values(Context &ctx, Function::ArgumentValues const &)
{
    return ctx.instanceScope().as<DictionaryValue>().contentsAsArray(DictionaryValue::Values);
}

static File const &fileInstance(Context &ctx)
{
    Record const *obj = ctx.instanceScope().as<RecordValue>().record();
    if(!obj)
    {
        throw Value::IllegalError("ScriptSystem::fileInstance", "No File instance available");
    }

    // The record is expected to have a path (e.g., File info record).
    return App::rootFolder().locate<File>(obj->gets("path", "/"));
}

static Value *Function_File_Locate(Context &ctx, Function::ArgumentValues const &args)
{
    Path const relativePath = args.at(0)->asText();

    if(File const *found = fileInstance(ctx).tryFollowPath(relativePath)->maybeAs<File>())
    {
        return new RecordValue(found->info());
    }

    // Wasn't there, result is None.
    return 0;
}

static Value *Function_File_Read(Context &ctx, Function::ArgumentValues const &)
{
    QScopedPointer<BlockValue> data(new BlockValue);
    fileInstance(ctx) >> *data;
    return data.take();
}

static Value *Function_File_ReadUtf8(Context &ctx, Function::ArgumentValues const &)
{
    Block raw;
    fileInstance(ctx) >> raw;
    return new TextValue(String::fromUtf8(raw));
}

static Animation &animationInstance(Context &ctx)
{
    Animation *obj = reinterpret_cast<Animation *>(ctx.instanceScope().as<NativeValue>().object());
    if(!obj)
    {
        throw Value::IllegalError("ScriptSystem::animationInstance", "No Animation instance available");
    }
    return *obj;
}

static Value *Function_Animation_Value(Context &ctx, Function::ArgumentValues const &)
{
    return new NumberValue(animationInstance(ctx).value());
}

static Value *Function_Animation_Target(Context &ctx, Function::ArgumentValues const &)
{
    return new NumberValue(animationInstance(ctx).target());
}

static Value *Function_Animation_SetValue(Context &ctx, Function::ArgumentValues const &args)
{
    animationInstance(ctx).setValue(float(args.at(0)->asNumber()), // value
                                    args.at(1)->asNumber(),        // span
                                    args.at(2)->asNumber());       // delay
    return nullptr;
}

static Value *Function_Animation_SetValueFrom(Context &ctx, Function::ArgumentValues const &args)
{
    animationInstance(ctx).setValueFrom(args.at(0)->asNumber(),    // fromValue
                                        args.at(1)->asNumber(),    // toValue
                                        args.at(2)->asNumber(),    // span
                                        args.at(3)->asNumber());   // delay
    return nullptr;
}

void initCoreModule(Binder &binder, Record &coreModule)
{
    // Dictionary
    {
        Record &dict = coreModule.addRecord("Dictionary");
        binder.init(dict)
                << DENG2_FUNC_NOARG(Dictionary_Keys, "keys")
                << DENG2_FUNC_NOARG(Dictionary_Values, "values");
    }

    // String
    {
        Record &str = coreModule.addRecord("String");
        binder.init(str)
                << DENG2_FUNC_NOARG(String_Upper, "upper")
                << DENG2_FUNC_NOARG(String_Lower, "lower")
                << DENG2_FUNC_NOARG(String_FileNamePath, "fileNamePath")
                << DENG2_FUNC_NOARG(String_FileNameExtension, "fileNameExtension")
                << DENG2_FUNC_NOARG(String_FileNameWithoutExtension, "fileNameWithoutExtension")
                << DENG2_FUNC_NOARG(String_FileNameAndPathWithoutExtension, "fileNameAndPathWithoutExtension");
    }

    // File
    {
        Record &file = coreModule.addRecord("File");
        binder.init(file)
                << DENG2_FUNC      (File_Locate, "locate", "relativePath")
                << DENG2_FUNC_NOARG(File_Read, "read")
                << DENG2_FUNC_NOARG(File_ReadUtf8, "readUtf8");
    }

    // Animation
    {
        Function::Defaults setValueDefaults;
        setValueDefaults["span"]  = new NumberValue(0.0);
        setValueDefaults["delay"] = new NumberValue(0.0);

        Function::Defaults setValueFromDefaults;
        setValueFromDefaults["delay"] = new NumberValue(0.0);

        Record &anim = coreModule.addRecord("Animation");
        binder.init(anim)
                << DENG2_FUNC_NOARG(Animation_Value, "value")
                << DENG2_FUNC_NOARG(Animation_Target, "target")
                << DENG2_FUNC_DEFS (Animation_SetValue, "setValue",
                                    "value" << "span" << "delay", setValueDefaults)
                << DENG2_FUNC_DEFS (Animation_SetValueFrom, "setValueFrom",
                                    "fromValue" << "toValue" << "span" << "delay", setValueFromDefaults);
    }
}

} // namespace de
