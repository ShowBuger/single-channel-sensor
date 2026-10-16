using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.WinForms;

internal static class ReleaseResources {
    internal static byte[] Read(string name) {
        using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("FIELD."+name)) {
            if(stream==null)throw new FileNotFoundException("Missing application resource: "+name);
            using(var unzip=new GZipStream(stream,CompressionMode.Decompress))using(var output=new MemoryStream()) {
                unzip.CopyTo(output);return output.ToArray();
            }
        }
    }
    internal static readonly string DataRoot=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"FIELD","Release");
}

internal static class ReleaseProgram {
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    static extern bool SetDllDirectory(string path);
    [STAThread]
    static int Main() {
        try {
            AppDomain.CurrentDomain.AssemblyResolve+=delegate(object sender,ResolveEventArgs args) {
                string name=new AssemblyName(args.Name).Name;
                if(name!="Microsoft.Web.WebView2.Core"&&name!="Microsoft.Web.WebView2.WinForms")return null;
                return Assembly.Load(ReleaseResources.Read(name+".dll"));
            };
            byte[] loader=ReleaseResources.Read("WebView2Loader.dll");
            string digest;
            using(var hash=SHA256.Create())digest=BitConverter.ToString(hash.ComputeHash(loader)).Replace("-","");
            string native=Path.Combine(ReleaseResources.DataRoot,"runtime",digest);
            Directory.CreateDirectory(native);
            string target=Path.Combine(native,"WebView2Loader.dll");
            if(!File.Exists(target)||!Equal(File.ReadAllBytes(target),loader))File.WriteAllBytes(target,loader);
            if(!SetDllDirectory(native))throw new InvalidOperationException("Cannot load WebView2 runtime.");
            // Release does not expose a remote debugging port, including through env overrides.
            Environment.SetEnvironmentVariable("WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",null);
            Environment.SetEnvironmentVariable("WEBVIEW2_PIPE_FOR_SCRIPT_DEBUGGER",null);
            Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
            return RunWindow();
        } catch(Exception error) {
            MessageBox.Show("无法启动 FIELD："+error.Message+"\n请确认已安装 Microsoft Edge WebView2 Runtime。", "FIELD",MessageBoxButtons.OK,MessageBoxIcon.Error);
            return 1;
        } finally { FieldBridge.StopRelease(); }
    }
    static bool Equal(byte[] a,byte[] b) { if(a.Length!=b.Length)return false;for(int i=0;i<a.Length;i++)if(a[i]!=b[i])return false;return true; }
    [MethodImpl(MethodImplOptions.NoInlining)]
    static int RunWindow() { FieldBridge.StartRelease();Application.Run(new ReleaseWindow());return 0; }
}

internal sealed class ReleaseWindow : Form {
    const string Origin="https://field.invalid";
    readonly WebView2 view=new WebView2();
    readonly JavaScriptSerializer json=new JavaScriptSerializer();
    bool closing;
    internal ReleaseWindow() {
        Text="FIELD · 传感监测";Size=new Size(1360,900);MinimumSize=new Size(800,600);
        StartPosition=FormStartPosition.CenterScreen;BackColor=Color.FromArgb(245,247,248);
        view.Dock=DockStyle.Fill;Controls.Add(view);
        Shown+=async delegate { await Initialize(); };
        FormClosed+=delegate { closing=true;FieldBridge.StopRelease();view.Dispose(); };
    }
    static bool Trusted(string source) {
        Uri uri;return Uri.TryCreate(source,UriKind.Absolute,out uri)&&uri.Scheme=="https"&&uri.Host=="field.invalid"&&uri.IsDefaultPort;
    }
    async Task Initialize() {
        try {
            var environment=await CoreWebView2Environment.CreateAsync(null,Path.Combine(ReleaseResources.DataRoot,"profile"));
            await view.EnsureCoreWebView2Async(environment);
            var core=view.CoreWebView2;
            core.Settings.AreDevToolsEnabled=false;
            core.Settings.AreDefaultContextMenusEnabled=false;
            core.Settings.AreBrowserAcceleratorKeysEnabled=false;
            core.Settings.IsStatusBarEnabled=false;
            core.Settings.IsBuiltInErrorPageEnabled=false;
            core.Settings.AreHostObjectsAllowed=false;
            core.Settings.IsPasswordAutosaveEnabled=false;
            core.Settings.IsGeneralAutofillEnabled=false;
            core.Settings.IsWebMessageEnabled=true;
            core.NewWindowRequested+=delegate(object sender,CoreWebView2NewWindowRequestedEventArgs args){args.Handled=true;};
            core.PermissionRequested+=delegate(object sender,CoreWebView2PermissionRequestedEventArgs args){args.State=CoreWebView2PermissionState.Deny;};
            core.NavigationStarting+=delegate(object sender,CoreWebView2NavigationStartingEventArgs args){if(!Trusted(args.Uri))args.Cancel=true;};
            core.AddWebResourceRequestedFilter("*",CoreWebView2WebResourceContext.All);
            core.WebResourceRequested+=ResourceRequested;
            core.WebMessageReceived+=MessageReceived;
            core.Navigate(Origin+"/");
        } catch(Exception error) {
            MessageBox.Show(this,"网页组件启动失败："+error.Message+"\n请安装 Microsoft Edge WebView2 Runtime 后重试。","FIELD",MessageBoxButtons.OK,MessageBoxIcon.Error);
            Close();
        }
    }
    void ResourceRequested(object sender,CoreWebView2WebResourceRequestedEventArgs args) {
        byte[] bytes=new byte[0];string type="text/plain; charset=utf-8";int status=404;
        if(Trusted(args.Request.Uri)&&args.Request.Method=="GET") {
            var uri=new Uri(args.Request.Uri);
            if(uri.AbsolutePath=="/"||uri.AbsolutePath=="/index.html") { bytes=ReleaseResources.Read("index.html");type="text/html; charset=utf-8";status=200; }
            else if(uri.AbsolutePath=="/bundle.js") { bytes=ReleaseResources.Read("bundle.js");type="text/javascript; charset=utf-8";status=200; }
        }
        const string policy="default-src 'none'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'none'; object-src 'none'; frame-src 'none'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'";
        args.Response=view.CoreWebView2.Environment.CreateWebResourceResponse(new MemoryStream(bytes),status,status==200?"OK":"Not Found",
            "Content-Type: "+type+"\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: "+policy+"\r\n");
    }
    async void MessageReceived(object sender,CoreWebView2WebMessageReceivedEventArgs args) {
        if(!Trusted(args.Source)||closing)return;
        object id=null,result=null;string error=null;
        try {
            string text=args.WebMessageAsJson;if(text.Length>16384)throw new ArgumentException("请求过长。");
            var request=json.Deserialize<Dictionary<string,object>>(text);
            id=request["id"];
            string operation=Convert.ToString(request["operation"]),connection=Convert.ToString(request["connection"]);
            var body=request["body"] as Dictionary<string,object>;
            result=await Task.Run(()=>FieldBridge.ReleaseCall(operation,body,connection));
        } catch(UnauthorizedAccessException) { error="串口被其他程序占用，请先关闭原上位机。"; }
        catch(Exception exception) { error=exception.Message; }
        if(!closing&&!IsDisposed&&view.CoreWebView2!=null)
            view.CoreWebView2.PostWebMessageAsJson(json.Serialize(new { id=id,ok=error==null,result=result,error=error }));
    }
}
