#!/usr/bin/env python3
"""Install the Twin Embers relay (tools/wl_relay.py) as a service that starts with the machine, on a Linux server with systemd
(Ubuntu or Oracle Linux, for instance the always-free virtual machine described in docs/RELAY-SERVER.md).

    sudo python3 install.py [port]        (run from the folder that has wl_relay.py, or from tools/relay-server in a checkout)

It makes a user without a login for the relay, copies the relay to /opt/twinembers-relay, writes the service twinembers-relay.service,
opens the port in the machine's own firewall, and starts it. Run it again to update the relay.
"""
import os
import pathlib
import shutil
import subprocess
import sys

UNIT = """[Unit]
Description=Twin Embers relay for online play
After=network-online.target
Wants=network-online.target

[Service]
User=twinembers-relay
ExecStart=/usr/bin/env python3 /opt/twinembers-relay/wl_relay.py --host 0.0.0.0 --port {port} --log
Restart=always
RestartSec=3
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
PrivateTmp=true
MemoryMax=256M

[Install]
WantedBy=multi-user.target
"""


def run(*cmd, check=True, quiet=False):
    return subprocess.run(cmd, check=check, stdout=subprocess.DEVNULL if quiet else None, stderr=subprocess.DEVNULL if quiet else None)


def have(cmd):
    return shutil.which(cmd) is not None


def open_firewall(port):
    """The machine's own firewall: Oracle's images ship iptables rules that refuse new ports; Ubuntu may use ufw; Oracle Linux firewalld."""
    if have('ufw') and b'active' in subprocess.run(['ufw', 'status'], capture_output=True).stdout:
        run('ufw', 'allow', '%d/tcp' % port)
    elif have('firewall-cmd') and subprocess.run(['firewall-cmd', '--state'], capture_output=True).returncode == 0:
        run('firewall-cmd', '--permanent', '--add-port=%d/tcp' % port)
        run('firewall-cmd', '--reload')
    elif have('iptables'):
        rule = ['INPUT', '-p', 'tcp', '--dport', str(port), '-j', 'ACCEPT']
        if run('iptables', '-C', *rule, check=False, quiet=True).returncode != 0:
            # before Oracle's final REJECT (its first rules let in established traffic, ping, loopback and SSH); first on a shorter chain
            if run('iptables', '-I', 'INPUT', '5', *rule[1:], check=False, quiet=True).returncode != 0:
                run('iptables', '-I', 'INPUT', '1', *rule[1:])
        if have('netfilter-persistent'):
            run('netfilter-persistent', 'save')


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 47311
    here = pathlib.Path(__file__).resolve().parent
    source = next((p for p in (here / 'wl_relay.py', here.parent / 'wl_relay.py') if p.exists()), None)
    if source is None:
        sys.exit('wl_relay.py not found next to this script or in tools/')
    if os.geteuid() != 0:
        sys.exit('run it with sudo')
    if subprocess.run(['id', 'twinembers-relay'], capture_output=True).returncode != 0:
        run('useradd', '--system', '--no-create-home', '--shell', '/usr/sbin/nologin', 'twinembers-relay')
    target = pathlib.Path('/opt/twinembers-relay')
    target.mkdir(mode=0o755, exist_ok=True)
    shutil.copyfile(source, target / 'wl_relay.py')
    (target / 'wl_relay.py').chmod(0o644)
    pathlib.Path('/etc/systemd/system/twinembers-relay.service').write_text(UNIT.format(port=port))
    open_firewall(port)
    run('systemctl', 'daemon-reload')
    run('systemctl', 'enable', 'twinembers-relay', quiet=True)
    run('systemctl', 'restart', 'twinembers-relay')
    run('sleep', '1')
    run('systemctl', '--no-pager', '--lines=5', 'status', 'twinembers-relay', check=False)
    print()
    print('The relay listens on port %d. Open the same port (TCP, ingress) in the cloud\'s own network rules too.' % port)
    print('Log: journalctl -u twinembers-relay -f')


if __name__ == '__main__':
    main()
