// HOST metadata only: use the actual compiled enum and StrictAccessor, not a
// replacement enum parser, shader interpreter or GPU capability provider.
using System.Collections;
using System.Reflection;
using System.Runtime.Loader;
using System.Text.Json;
if (args.Length != 2) throw new ArgumentException("TextureFormats FRONTEND_DLL INVENTORY_JSON");
string front = Path.GetFullPath(args[0]);
AssemblyLoadContext.Default.Resolving += (context, name) =>
{
    string path = Path.Combine(Path.GetDirectoryName(front)!, name.Name + ".dll");
    return File.Exists(path) ? context.LoadFromAssemblyPath(path) : null;
};
Assembly assembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(front);
Type accessor = assembly.GetType("StrictAccessor", throwOnError: true)!;
ConstructorInfo constructor = accessor.GetConstructor([typeof(byte[]), typeof(byte[]), typeof(JsonElement)])!;
MethodInfo query = accessor.GetMethod("QueryTextureFormat")!, cap = accessor.GetMethod("QueryHostSupportsSnormBufferTextureFormat")!;
FieldInfo queries = accessor.GetField("Queries")!;
Type formatType = query.ReturnType;
int checks = 0, accepted = 0, rejected = 0;
void Check(bool value) { checks++; if (!value) throw new InvalidDataException("check " + checks); }
object New(object? format, string type = "TextureBuffer", bool missing = false, bool duplicate = false)
{
    Dictionary<string, object?> row = new() { ["handle"] = 112, ["cbuf_slot"] = -1, ["sampler_type"] = type };
    if (!missing) row["texture_format"] = format;
    var rows = duplicate ? new[] { row, row } : new[] { row };
    var state = JsonSerializer.SerializeToElement(new { resources = new { textures = rows }, host = new { QueryHostSupportsSnormBufferTextureFormat = false } });
    return constructor.Invoke([Array.Empty<byte>(), Array.Empty<byte>(), state]);
}
void Reject(object target, int handle = 112, int cbufSlot = -1)
{
    try { query.Invoke(target, [handle, cbufSlot]); }
    catch (TargetInvocationException e) when (e.InnerException is NotSupportedException or InvalidDataException)
    { Check(((IList)queries.GetValue(target)!).Count == 0); rejected++; return; }
    throw new InvalidDataException("invalid texture-format input accepted");
}
using var document = JsonDocument.Parse(File.ReadAllText(args[1]));
Check(Enum.GetValues(formatType).Length == 40);
Check(document.RootElement.GetArrayLength() == 39);
var output = new List<object>();
foreach (JsonElement row in document.RootElement.EnumerateArray())
{
    int value = row.GetProperty("shader").GetInt32();
    string name = row.GetProperty("shader_name").GetString()!;
    Check(value > 0 && value < 40 && Enum.IsDefined(formatType, value));
    Check(Enum.GetName(formatType, value) == name);
    object target = New(name);
    Check(Convert.ToInt32(query.Invoke(target, [112, -1])) == value);
    IList trace = (IList)queries.GetValue(target)!;
    Check(trace.Count == 1);
    JsonElement captured = JsonSerializer.SerializeToElement(trace[0]);
    Check(captured.GetProperty("name").GetString() == "QueryTextureFormat");
    Check(captured.GetProperty("handle").GetInt32() == 112 && captured.GetProperty("cbufSlot").GetInt32() == -1);
    Check(captured.GetProperty("value").GetInt32() == value);
    Check(cap.Invoke(target, null) is false);
    Check(trace.Count == 2 && JsonSerializer.SerializeToElement(trace[1]).GetProperty("value").ValueKind == JsonValueKind.False);
    output.Add(new { nvn = row.GetProperty("nvn").GetInt32(), shader = value, name, host_snorm = false });
    accepted++;
}
foreach (object? invalid in new object?[] { null, 0, 34, 37, -1, true, new[] { "R32Float" }, "", "Unknown", "0", "34", "37", "-1", "0x22", "NVN_FORMAT_RGBA32F", "RGBA32F", "FMT6_32_32_32_32_FLOAT", "PIPE_FORMAT_R32G32B32A32_FLOAT", "SpvImageFormatRgba32f", "r32float", " R32Float", "R32Float ", "R32Float,R8Unorm", "R64Float" }) Reject(New(invalid));
Reject(New("R32Float", missing: true));
Reject(New("R32Float", duplicate: true));
Reject(New("R32Float", "Texture2D"));
Reject(New("R32Float", "Texture1D"));
Reject(New("R32Float", "TextureCube"));
Reject(New("R32Float"), handle: 111);
Reject(New("R32Float"), cbufSlot: 2);
Check(accepted == 39 && rejected >= 24);
Console.WriteLine(JsonSerializer.Serialize(new { passed = true, checks, accepted, rejected, host_snorm = false, gpu_execution = false, public_buffer_gpu_consumer = false, formats = output }));
