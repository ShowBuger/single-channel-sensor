// Built with the C# compiler supplied by Windows .NET Framework. No NuGet.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Web.Script.Serialization;

internal static class FieldBridge {
    static readonly object Gate = new object();
    static readonly Queue<byte> Received = new Queue<byte>();
    static readonly AutoResetEvent DataReady = new AutoResetEvent(false);
    static readonly JavaScriptSerializer Json = new JavaScriptSerializer();
    static readonly string Root = AppDomain.CurrentDomain.BaseDirectory;
    static readonly string Key = RandomKey();
    static SerialPort serial;
    static string connection = "", lastError = "";
    static DateTime lastPoll = DateTime.UtcNow;
    static TcpListener listener;
    static volatile bool alive = true;
    static int httpPort;
    static string Origin { get { return "http://127.0.0.1:" + httpPort; } }
    static string RandomKey() { var b = new byte[32]; using (var rng = RandomNumberGenerator.Create()) rng.GetBytes(b); return Convert.ToBase64String(b); }
    static string Encode(object data) { lock (Json) return Json.Serialize(data); }
    static Dictionary<string,object> Decode(string text) { lock (Json) return Json.Deserialize<Dictionary<string,object>>(text); }
    static int Main(string[] args) {
        try {
            int requested = 8766;
            var portArg = args.FirstOrDefault(x => x.StartsWith("--port="));
            if (portArg != null) requested = int.Parse(portArg.Substring(7));
            if (!File.Exists(Path.Combine(Root,"index.html"))) throw new Exception("Keep FIELD.exe alongside index.html, core.js, app.js and bridge-client.js.");
            listener = new TcpListener(IPAddress.Loopback, requested);
            try { listener.Start(); } catch(SocketException) { if (portArg != null) throw; listener = new TcpListener(IPAddress.Loopback,0); listener.Start(); }
            httpPort = ((IPEndPoint)listener.LocalEndpoint).Port;
            Console.WriteLine("FIELD local serial service: " + Origin);
            Console.WriteLine("Close this window to exit. No internet connection required.");
            Console.CancelKeyPress += delegate(object s, ConsoleCancelEventArgs e) { e.Cancel = true; Stop(); };
            new Thread(Watchdog) { IsBackground = true }.Start();
            if (!args.Contains("--no-browser")) Process.Start(new ProcessStartInfo(Origin) { UseShellExecute = true });
            while (alive) { try { var client = listener.AcceptTcpClient(); ThreadPool.QueueUserWorkItem(_ => Handle(client)); } catch(SocketException) { if (alive) throw; } }
            return 0;
        } catch(Exception e) { Console.Error.WriteLine(e.Message); return 1; }
        finally { Disconnect(null); }
    }
    static void Stop() { alive = false; listener.Stop(); Disconnect(null); }
    static void Watchdog() { while(alive) { Thread.Sleep(1000); lock(Gate) { if(serial != null && (DateTime.UtcNow-lastPoll).TotalSeconds > 20) Disconnect("页面停止响应，串口已自动释放。"); } } }
    static string[] Ports() { return SerialPort.GetPortNames().Distinct().OrderBy(p => { int n; return int.TryParse(p.Substring(3),out n) ? n : int.MaxValue; }).ToArray(); }
    static object Connect(string port, int baud) {
        lock(Gate) {
            if(serial != null) throw new InvalidOperationException("串口正在使用，请在原页面断开后重试。");
            if(!Ports().Contains(port)) throw new ArgumentException("端口已移除，请刷新端口列表。");
            if(!new[]{9600,57600,115200,230400,460800}.Contains(baud)) throw new ArgumentException("不支持的波特率。");
            var candidate = new SerialPort(port,baud,Parity.None,8,StopBits.One) { Handshake=Handshake.None,ReadTimeout=250,WriteTimeout=2000,ReadBufferSize=65536,DtrEnable=false,RtsEnable=false };
            try { candidate.Open(); } catch { candidate.Dispose(); throw; }
            serial=candidate; connection=Guid.NewGuid().ToString("N");lastError="";lastPoll=DateTime.UtcNow;Received.Clear();
            new Thread(()=>ReadSerial(candidate)) { IsBackground=true }.Start();
            return new { connection=connection, port=port, baudRate=baud };
        }
    }
    static void ReadSerial(SerialPort own) {
        var buffer=new byte[8192];
        while(alive) {
            lock(Gate) { if(serial != own) return; }
            try {
                int n=own.Read(buffer,0,buffer.Length);
                lock(Gate) { if(serial != own) return; if(Received.Count+n>1048576) { Disconnect("接收缓存溢出，连接已关闭，请重新连接。");return; } for(int i=0;i<n;i++) Received.Enqueue(buffer[i]); }
                DataReady.Set();
            } catch(TimeoutException) { } catch(Exception e) { lock(Gate) { if(serial==own) Disconnect("串口读取中断："+e.Message); } return; }
        }
    }
    static void Disconnect(string error) {
        lock(Gate) { var old=serial;serial=null;if(error!=null)lastError=error;try { if(old!=null)old.Dispose(); } catch { } DataReady.Set(); }
    }
    static void CheckConnection(string id) { if(string.IsNullOrEmpty(id)||id!=connection) throw new InvalidOperationException("串口会话已改变，请重新连接。"); }
    static object Read(string id) {
        lock(Gate) { CheckConnection(id);lastPoll=DateTime.UtcNow; }
        DataReady.WaitOne(250);
        lock(Gate) { CheckConnection(id);var bytes=Received.ToArray();Received.Clear();return new { data=Convert.ToBase64String(bytes), connected=serial!=null,error=lastError }; }
    }
    static void Write(string id,string command) {
        // Only the existing firmware protocol is accepted; each request is one line.
        if(!ValidCommand(command)) throw new ArgumentException("无效的设备命令。");
        lock(Gate) { CheckConnection(id);if(serial==null)throw new InvalidOperationException("串口已断开。");serial.Write(command+"\n"); }
    }
    static bool ValidCommand(string command) {
        if(command==null||command.Length>62||command.Any(c=>c<32||c>126))return false;
        if(Regex.IsMatch(command,@"^ML (START|STOP|CALIB|FLAG|MAPPING|MERSURE|LOGLEVEL|GETCFG|IMPORT_END|IMPORT_CANCEL|CHFLAG (0x[0-9A-Fa-f]{1,2}|[0-9]{1,3})|SETMAP [XYZ] [+-]?[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)$"))return true;
        var start=Regex.Match(command,@"^ML IMPORT_START ([0-9]{1,4})$");
        if(start.Success){int n=int.Parse(start.Groups[1].Value);return n>0&&n<=1023;}
        return Regex.IsMatch(command,"^ML IMPORT_DATA [A-Za-z0-9_{}\":,.+ -]{1,47}$");
    }
    static string Value(Dictionary<string,object> data,string name) { object value;return data!=null&&data.TryGetValue(name,out value)?Convert.ToString(value):""; }
    static void Handle(TcpClient client) {
        using(client) {
            client.ReceiveTimeout=5000;client.SendTimeout=5000;
            try { using(var stream=client.GetStream()) {
                // Small HTTP/1.1 subset: bounded headers + Content-Length; close each response.
                var header=new List<byte>();int value;
                while((value=stream.ReadByte())!=-1) { header.Add((byte)value);int n=header.Count;if(n>16384)throw new ArgumentException("Request headers too large");if(n>=4&&header[n-4]==13&&header[n-3]==10&&header[n-2]==13&&header[n-1]==10)break; }
                var lines=Encoding.ASCII.GetString(header.ToArray()).Split(new[]{"\r\n"},StringSplitOptions.None);
                var first=lines[0].Split(' ');if(first.Length!=3)return;
                string method=first[0],path=first[1].Split('?')[0];
                var headers=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
                foreach(var line in lines.Skip(1)) { int colon=line.IndexOf(':');if(colon>0)headers[line.Substring(0,colon)]=line.Substring(colon+1).Trim(); }
                string host,origin,site;
                if(!headers.TryGetValue("Host",out host)||host!="127.0.0.1:"+httpPort) { Reply(stream,403,new{error="Invalid host"});return; }
                if((headers.TryGetValue("Origin",out origin)&&origin!=Origin)||(headers.TryGetValue("Sec-Fetch-Site",out site)&&site=="cross-site")) { Reply(stream,403,new{error="Cross-origin access denied"});return; }
                if(path.StartsWith("/api/")) {
                    if(path=="/api/session"&&method=="GET") { Reply(stream,200,new{token=Key,version="1.0",transport="windows-serial"});return; }
                    string auth;if(!headers.TryGetValue("X-Field-Key",out auth)||auth!=Key) { Reply(stream,403,new{error="Invalid session token"});return; }
                    int length=0;string len;if(headers.ContainsKey("Transfer-Encoding")||(headers.TryGetValue("Content-Length",out len)&&(!int.TryParse(len,out length)||length<0||length>16384))) { Reply(stream,400,new{error="Invalid request body"});return; }
                    var body=new byte[length];int count=0;while(count<length) { int n=stream.Read(body,count,length-count);if(n==0)throw new IOException("Incomplete body");count+=n; }
                    var data=length>0?Decode(Encoding.UTF8.GetString(body)):null;string id;headers.TryGetValue("X-Field-Connection",out id);
                    try {
                        if(path=="/api/ports"&&method=="GET")Reply(stream,200,new{ports=Ports()});
                        else if(path=="/api/connect"&&method=="POST")Reply(stream,200,Connect(Value(data,"port"),int.Parse(Value(data,"baudRate"))));
                        else if(path=="/api/read"&&method=="GET")Reply(stream,200,Read(id));
                        else if(path=="/api/write"&&method=="POST") { Write(id,Value(data,"command"));Reply(stream,200,new{ok=true}); }
                        else if(path=="/api/disconnect"&&method=="POST") { lock(Gate) { CheckConnection(id);Disconnect(""); }Reply(stream,200,new{ok=true}); }
                        else Reply(stream,404,new{error="Endpoint not found"});
                    } catch(UnauthorizedAccessException) { Reply(stream,409,new{error="串口被其他程序占用或访问被拒绝，请先关闭原上位机。"}); }
                    catch(Exception e) { Reply(stream,400,new{error=e.Message}); }
                    return;
                }
                if(method!="GET") { Reply(stream,405,new{error="Method not allowed"});return; }
                string file=path=="/"?"index.html":path.TrimStart('/');
                if(!new[]{"index.html","app.js","core.js","bridge-client.js","import-ui.js"}.Contains(file)) { Reply(stream,404,new{error="File not found"});return; }
                var bytes=File.ReadAllBytes(Path.Combine(Root,file));Respond(stream,200,file.EndsWith(".js")?"text/javascript; charset=utf-8":"text/html; charset=utf-8",bytes);
            } } catch { /* A closed browser socket must not stop the listener. */ }
        }
    }
    static void Reply(Stream stream,int code,object data) { Respond(stream,code,"application/json; charset=utf-8",Encoding.UTF8.GetBytes(Encode(data))); }
    static void Respond(Stream stream,int code,string type,byte[] bytes) {
        string headers="HTTP/1.1 "+code+" "+(code==200?"OK":"Error")+"\r\nContent-Type: "+type+"\r\nContent-Length: "+bytes.Length+"\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nX-Frame-Options: DENY\r\nContent-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; object-src 'none'; frame-ancestors 'none'; base-uri 'none'\r\n\r\n";
        var head=Encoding.ASCII.GetBytes(headers);stream.Write(head,0,head.Length);stream.Write(bytes,0,bytes.Length);
    }
}
