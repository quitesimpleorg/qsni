use std::env;
use std::io::BufReader;
use std::io::BufRead;
use std::io::Error;
use std::io::ErrorKind;
use std::io::Write;
use std::ffi::CString;

static PROFILEPATH : &'static str ="/etc/qsni.d/";
static NET_CLS_DIR : &'static str="/sys/fs/cgroup/net_cls/";
extern crate nix;


fn ensure_outside_profile()
{
    let fp = std::fs::File::open("/proc/self/cgroup").expect("Error opening cgroups file of process");
    let bf = BufReader::new(fp);
    for line in bf.lines() {
        let currentline = line.expect("Error while reading line");
        let splitted : Vec<&str> = currentline.split(':').collect();
        if splitted.len() < 3 {
            panic!("Misformated line in cgroups file!");
            }
        if splitted[1] == "net_cls"  && splitted[2] != "/" {
           panic!("already assigned to a net class, thus you can't use this binary to change that");
        }
    }
}

fn init_profile(profilepath : &str)
{
    use nix::unistd::*;
    use nix::sys::wait::*;
    match fork()
        {
            Ok(ForkResult::Parent { child, .. }) => {
               let waitresult =  waitpid(child, Some(WaitPidFlag::empty())).expect("waitpid failed");
               match waitresult
                   {
                      WaitStatus::Exited(pid, code) =>
                          {
                              if code != 0
                                  {
                                      panic!("profile setup script failed");
                                  }
                          },
                       _ => { },
                   }
            }
            Ok(ForkResult::Child) => {
                unsafe { nix::libc::clearenv(); }
                nix::unistd::execv(&CString::new(profilepath).unwrap(), &[CString::new(profilepath).unwrap()]).expect("Faileed execv");},
            Err(_) => println!("Fork failed"),
        }
}

fn assign_to_profile(profilename : &str)
{
    let filename = NET_CLS_DIR.to_owned() + "/" + profilename + "/tasks";
    let mut file = std::fs::OpenOptions::new().write(true).append(true).open(filename).expect("Failed to open net class file for writing");
    let mypid = nix::unistd::getpid().to_string();
    write!(file, "{}", mypid).expect("An error occured while writing the pid");

}

fn main()
{
    std::panic::set_hook(Box::new(|pi| {
        if let Some(s) = pi.payload().downcast_ref::<String>() {
            eprintln!("{}", s);
        }
        eprintln!("Details:");
        eprintln!("{}", pi);
    }));
    let args : Vec<String> = std::env::args().collect();
    if args.len() < 3 {
        println!("usage: qsni profile command [arguments...]");
        std::process::exit(1);
        }
    ensure_outside_profile();

    let profilename : &str = &args[1];

    let profilefilepath = PROFILEPATH.to_owned() + profilename;
    if ! std::path::Path::new(&profilefilepath).exists(){
            eprintln!("The specified profile {} does not exist", profilename);
            std::process::exit(1);
        }

    let currentuid = nix::unistd::getuid();
    let currentgid = nix::unistd::getgid();

    nix::unistd::setuid(nix::unistd::Uid::from_raw(0)).expect("Failed to become root");

    init_profile(&profilefilepath);
    assign_to_profile(&profilename);

    nix::unistd::setgid(currentgid).expect("setgid failed during drop");
    nix::unistd::setuid(currentuid).expect("setuid failed during drop");

    let cmd = &args[2];
    let mut execargs : Vec<CString> = args.iter().skip(3).map(|s| { CString::new(s.as_str()).unwrap() }).collect();
    execargs.insert(0, CString::new(cmd.as_str()).unwrap());
    nix::unistd::execvp(&CString::new(cmd.as_str()).unwrap(), &execargs).expect("execv failed launching your program");
}
