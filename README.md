qsni¹
====
qsni (quite simple network isolation) allows for simple assignment
of per cgroup iptables rules to programs.

While you can also achieve this (and more) using network namespaces,
the setup is not as simple/easy.

Requirements
------------
You need an iptables version that supports cgroup matching (e. g. 
version >= 1.6);

The following kernel config paramaters must be set:
CONFIG_NETFILTER_XT_MATCH_CGROUP
CONFIG_NET_CLS_CGROUP

Example
-------
```
$ qsni blocked ping google.com
ping: unknown host google.com
```

```
$ qsni lan bash
$ ping 8.8.8.8
PING 8.8.8.8 (8.8.8.8) 56(84) bytes of data.
ping: sendmsg: Operation not permitted
$ ping 192.168.1.1
PING 192.168.1.1 (192.168.1.1) 56(84) bytes of data.
64 bytes from 192.168.1.1: icmp_seq=1 ttl=64 time=0.127 ms
$ qsni someprofile bash
already assigned to a net class, thus you can't use this binary to change that
$
```

Setup
-----
If cgroup_root isn't mounted to /sys/fs/cgroup, do it or change the 
constant in the source to the correct path.

make 
cp qsni /usr/bin/
chmod o=rx /usr/bin/qsni
chown root:root /usr/bin/qsni
setcap 'cap_setuid=ep cap_setgid=ep' /usr/bin/qsni

mkdir /etc/qsni.d
chmod o=rx /etc/qsni.d
cp profiles/blocked /etc/qsni.d/blocked
chmod o=r /etc/qsni.d/blocked

Every profile must have its own unique CGROUP_ID value in the profile 
file.


Security discussion
--------------------
This alone is not a satisfactory way to prevent misbehaving programs
to contact destinations you don't want them to. While the restrictions
also apply to the children of the launched progorams, at a minimum, file
system isolation is also necessary and perhaps IPC etc.

qsni however does not aim to be a complete "jailing/isolation" solution.
Nevertheless, I have use cases for it, hence its existence.

¹ name is preliminary, 
