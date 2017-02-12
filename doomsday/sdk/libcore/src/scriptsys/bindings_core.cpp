/** @file bindings_core.cpp
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/AnimationValue"
#include "de/App"
#include "de/BlockValue"
#include "de/Context"
#include "de/DictionaryValue"
#include "de/Folder"
#include "de/Function"
#include "de/NativePointerValue"
#include "de/Path"
#include "de/Record"
#include "de/RecordValue"
#include "de/TextValue"
#include "de/TimeValue"

namespace de {

static Value *Function_String_FileNamePath(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.nativeSelf().asText().fileNamePath());
}

static Value *Function_String_FileNameExtension(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.nativeSelf().asText().fileNameExtension());
}

static Value *Function_String_FileNameWithoutExtension(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.nativeSelf().asText().fileNameWithoutExtension());
}

static Value *Function_String_FileNameAndPathWithoutExtension(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.nativeSelf().asText().fileNameAndPathWithoutExtension());
}

static Value *Function_String_Upper(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.nativeSelf().asText().upper());
}

static Value *Function_String_Lower(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.nativeSelf().asText().lower());
}

//---------------------------------------------------------------------------------------

static Value *Function_Path_WithoutFileName(Context &, Function::ArgumentValues const &args)
{
    return new TextValue(args.at(0)->asText().fileNamePath());
}

//---------------------------------------------------------------------------------------

static Value *Function_Dictionary_Keys(Context &ctx, Function::ArgumentValues const &)
{
    return ctx.nativeSelf().as<DictionaryValue>().contentsAsArray(DictionaryValue::Keys);
}

static Value *Function_Dictionary_Values(Context &ctx, Function::ArgumentValues const &)
{
    return ctx.nativeSelf().as<DictionaryValue>().contentsAsArray(DictionaryValue::Values);
}

//---------------------------------------------------------------------------------------

static File const &fileInstance(Context &ctx)
{
    // The record is expected to have a path (e.g., File info record).
    File *file = ctx.selfInstance().get(Record::VAR_NATIVE_SELF)
            .as<NativePointerValue>().nativeObject<File>();
    if (!file)
    {
        throw Value::IllegalError("ScriptSystem::fileInstance", "Not a File instance");
    }
    return *file;
}

static Value *Function_File_Name(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(fileInstance(ctx).name());
}

static Value *Function_File_Path(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(fileInstance(ctx).path());
}

static Value *Function_File_Type(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(fileInstance(ctx).status().type() == File::Status::FILE? "file" : "folder");
}

static Value *Function_File_Size(Context &ctx, Function::ArgumentValues const &)
{
    return new NumberValue(fileInstance(ctx).size());
}

static Value *Function_File_ModifiedAt(Context &ctx, Function::ArgumentValues const &)
{
    return new TimeValue(fileInstance(ctx).status().modifiedAt);
}

static Value *Function_File_Locate(Context &ctx, Function::ArgumentValues const &args)
{
    Path const relativePath = args.at(0)->asText();

    if (File const *found = fileInstance(ctx).tryFollowPath(relativePath)->maybeAs<File>())
    {
        return new RecordValue(found->objectNamespace());
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

static Value *Function_File_MetaId(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(fileInstance(ctx).metaId().asHexadecimalText());
}

static Value *Function_Folder_List(Context &ctx, Function::ArgumentValues const &)
{
    Folder const &folder = fileInstance(ctx).as<Folder>();
    std::unique_ptr<ArrayValue> array(new ArrayValue);
    foreach (String name, folder.contents().keys())
    {
        *array << new TextValue(name);
    }
    return array.release();
}

static Value *Function_Folder_ContentSize(Context &ctx, Function::ArgumentValues const &)
{
    Folder const &folder = fileInstance(ctx).as<Folder>();
    return new NumberValue(folder.contents().size());
}

//---------------------------------------------------------------------------------------

static Animation &animationInstance(Context &ctx)
{
    if (AnimationValue *v = ctx.nativeSelf().maybeAs<AnimationValue>())
    {
        return v->animation();
    }
    // Could also just be a pointer to an Animation.
    Animation *obj = ctx.nativeSelf().as<NativePointerValue>().nativeObject<Animation>();
    if (!obj)
    {
        throw Value::IllegalError("ScriptSystem::animationInstance", "Not an Animation instance");
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

//---------------------------------------------------------------------------------------

void initCoreModule(Binder &binder, Record &coreModule)
{
    // Dictionary
    {

        Record &dict = coreModule.addSubrecord("Dictionary")
                .setFlags(Record::WontBeDeleted); // optimize: nobody needs to observe deletion
        binder.init(dict)
                << DENG2_FUNC_NOARG(Dictionary_Keys, "keys")
                << DENG2_FUNC_NOARG(Dictionary_Values, "values");
    }

    // String
    {
        Record &str = coreModule.addSubrecord("String").setFlags(Record::WontBeDeleted);
        binder.init(str)
                << DENG2_FUNC_NOARG(String_Upper, "upper")
                << DENG2_FUNC_NOARG(String_Lower, "lower")
                << DENG2_FUNC_NOARG(String_FileNamePath, "fileNamePath")
                << DENG2_FUNC_NOARG(String_FileNameExtension, "fileNameExtension")
                << DENG2_FUNC_NOARG(String_FileNameWithoutExtension, "fileNameWithoutExtension")
                << DENG2_FUNC_NOARG(String_FileNameAndPathWithoutExtension, "fileNameAndPathWithoutExtension");
    }

    // Path
    {
        Record &path = coreModule.addSubrecord("Path").setFlags(Record::WontBeDeleted);
        binder.init(path)
                << DENG2_FUNC(Path_WithoutFileName, "withoutFileName", "path");
    }

    // File
    {
        Record &file = coreModule.addSubrecord("File").setFlags(Record::WontBeDeleted);
        binder.init(file)
                << DENG2_FUNC_NOARG(File_Name, "name")
                << DENG2_FUNC_NOARG(File_Path, "path")
                << DENG2_FUNC_NOARG(File_Type, "type")
                << DENG2_FUNC_NOARG(File_Size, "size")
                << DENG2_FUNC_NOARG(File_ModifiedAt, "modifiedAt")
                << DENG2_FUNC      (File_Locate, "locate", "relativePath")
                << DENG2_FUNC_NOARG(File_Read, "read")
                << DENG2_FUNC_NOARG(File_ReadUtf8, "readUtf8")
                << DENG2_FUNC_NOARG(File_MetaId, "metaId");
    }

    // Folder
    {
        Record &folder = coreModule.addSubrecord("Folder").setFlags(Record::WontBeDeleted);
        binder.init(folder)
                << DENG2_FUNC_NOARG(Folder_List, "list")
                << DENG2_FUNC_NOARG(Folder_ContentSize, "contentSize");
    }

    // Animation
    {
        Function::Defaults setValueArgs;
        setValueArgs["span"]  = new NumberValue(0.0);
        setValueArgs["delay"] = new NumberValue(0.0);

        Function::Defaults setValueFromArgs;
        setValueFromArgs["delay"] = new NumberValue(0.0);

        Record &anim = coreModule.addSubrecord("Animation").setFlags(Record::WontBeDeleted);
        binder.init(anim)
                << DENG2_FUNC_NOARG(Animation_Value, "value")
                << DENG2_FUNC_NOARG(Animation_Target, "target")
                << DENG2_FUNC_DEFS (Animation_SetValue, "setValue",
                                    "value" << "span" << "delay", setValueArgs)
                << DENG2_FUNC_DEFS (Animation_SetValueFrom, "setValueFrom",
                                    "fromValue" << "toValue" << "span" << "delay", setValueFromArgs);
    }
}

} // namespace de
