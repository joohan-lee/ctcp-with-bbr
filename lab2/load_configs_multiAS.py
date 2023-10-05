r"""
This python file
1. Read zebra.conf.sav, ospfd.conf.sav, and bgpd.conf.sav from /lab2/configs/{router_name}
2. and load them automatically to each router appropriately.

ref: vtysh manual. https://linux.die.net/man/1/vtysh
ref: ./go_to.sh
"""
"""
vtysh 
Synopsis
vtysh [ -b ]
vtysh [ -E ] [ -d daemon ] ] [ -c command ]

Options
-b, --boot
Execute boot startup configuration. It makes sense only if integrated config file is in use (not default in Quagga). See Info file Quagga for more info.
-c, --command command
Specify command to be executed under batch mode. It behaves like -c option in any other shell - command is executed and vtysh exits.
It's useful for gathering info from Quagga routing software or reconfiguring daemons from inside shell scripts, etc. Note that multiple commands may be executed by using more than one -c option and/or embedding linefeed characters inside the command string.

-d, --daemon daemon_name
Specify which daemon to connect to. By default, vtysh attempts to connect to all Quagga daemons running on the system. With this flag, one can specify a single daemon to connect to instead. For example, specifying '-d ospfd' will connect only to ospfd. This can be particularly useful inside scripts with -c where the command is targeted for a single daemon.
-e, --execute command
Alias for -c. It's here only for compatibility with Zebra routing software and older Quagga versions. This will be removed in future.
-E, --echo
When the -c option is being used, this flag will cause the standard vtysh prompt and command to be echoed prior to displaying the results. This is particularly useful to separate the results when executing multiple commands.
-h, --help
Display a usage message on standard output and exit.
"""
import sys
import os
import subprocess

ROUTER_NAMES= ["ATLA", "CHIC", "HOUS", "KANS", "LOSA", "NEWY", "SALT", "SEAT", "WASH", "west", "east"]
ZEBRA_CONF_FILENAME = "zebra.conf.sav"
OSPFD_CONF_FILENAME = "ospfd.conf.sav"
BGPD_CONF_FILENAME = "bgpd.conf.sav"

def generate_command_from_config(file):
    with open(file, 'r') as f:
        lines = f.readlines()
    
    exclude_set = set(['hostname ', 'password ', 'ip forwarding', 'ipv6 forwarding', 'line vty'])
    commands = []
    command = []
    for i, line in enumerate(lines):
        # a set of commands(configuration) is between ! and !.
        # For example, below are conf file.
        # !
        # interface hous
        #  ip address 4.0.7.1/24
        # !
        # interface wash
        #  ip ospf cost 700
        # !
        # router ospf
        #  network 4.0.3.0/24 area 0.0.0.0
        #  network 4.0.5.0/24 area 0.0.0.0
        #  network 4.0.7.0/24 area 0.0.0.0
        #  network 4.104.0.0/24 area 0.0.0.0
        # !
        
        if not line.startswith('!') and (not line.strip() in exclude_set) \
            and (not (line.startswith('hostname') \
                    or line.startswith('password') \
                    or line.startswith('log file'))):
            command.append(line.strip())
        else:
            if len(command) > 1:
                commands.append(command)
            command = []
    return commands


def get_pid_mx_host(host):
    """
    This function referred to go_to.sh.
    Find pid of running mininet router.
    """
    command = 'ps ax | grep "mininet:{}$" | grep bash | grep -v mxexec | awk \'{{print $1}}\''.format(host)

    # Run the command
    process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    # Get the output and error
    output, error = process.communicate()

    # Check if there was an error, otherwise return pid
    if process.returncode != 0:
        print("Error:", error)
    else:
        pid = output.strip()
        if pid == '':
            raise Exception('Could not find Mininet host {}'.format(host))
        if ' ' in pid:
            raise Exception('Error: found multiple mininet:{} processes'.format(host))
        return pid

def run_vty_command(router, conf_cmds):
    """
    This function referred to go_to.sh
    ref. https://docs.python.org/2.7/library/subprocess.html#subprocess.check_call
    """

    pid = get_pid_mx_host(router)

    cgroup='/sys/fs/cgroup/cpu/%s' % (router)
    if os.path.exists(cgroup):
        cg = '-g %s' % (router)
    else:
        cg = ''

    # vty_cmd = 'vtysh -c \"%s\"' % (conf_cmd)
    vty_cmd = '"vtysh'
    vty_cmd += ' -c \\"conf t\\"'
    for conf_cmd in conf_cmds:
        for l in conf_cmd:
            vty_cmd += ' -c \\"' + l + '\\"'
        vty_cmd += ' -c \\"exit\\"'
    vty_cmd += ' -c \\"exit\\"'
    vty_cmd += '"'

    # Test
    # vty_cmd += ' -c \\"show run\\"'
    # vty_cmd += '"'
    
    rootdir="/var/run/mn/%s/root" % (router)
    if os.path.exists(rootdir) and os.access(rootdir + '/bin/bash', os.X_OK):
        vty_cmd = "chroot %s /bin/bash -c 'cd `pwd`; exec %s'" % (rootdir, vty_cmd)

    
    vty_cmd = "exec sudo mxexec -a %s -b %s -k %s bash -c %s %s" % (pid, pid, pid, cg, vty_cmd)
    # vty_cmd = "exec sudo mxexec -a %s -b %s -k %s -c %s %s" % (pid, pid, pid, cg, vty_cmd)
    
    # Examples
    # ./go_to.sh ATLA => e.g., exec sudo mxexec -a 4046 -b 4046 -k 4046 bash
    # exec sudo mxexec -a 4046 -b 4046 -k 4046 bash -c "vtysh -c \"show run\""
    # ./go_to.sh NEWY -c "vtysh -c \"show run\"" => exec sudo mxexec -a 4088 -b 4088 -k 4088 -c vtysh -c "show run"
    try:
        # print('subprocess started')
        subprocess.check_call(vty_cmd, shell=True)
        # print('subprocess ended')
    except subprocess.CalledProcessError as e:
        print("Error: %s" % e)

if __name__ == "__main__":
    # How to run: python load_configs.py configs_multiAS
    CONFIG_PATH = sys.argv[1]
    for router_name in ROUTER_NAMES:
        router_path = os.path.join(CONFIG_PATH, router_name)
        zebra_conf = os.path.join(router_path, ZEBRA_CONF_FILENAME)
        ospfd_conf = os.path.join(router_path, OSPFD_CONF_FILENAME)
        bgpd_conf = os.path.join(router_path, BGPD_CONF_FILENAME)

        zebra_commands = generate_command_from_config(zebra_conf)
        ospfd_commands = generate_command_from_config(ospfd_conf)
        bgpd_commands = generate_command_from_config(bgpd_conf)

        run_vty_command(router_name, zebra_commands)
        run_vty_command(router_name, ospfd_commands)
        run_vty_command(router_name, bgpd_commands)
    
    print('Configured all the routers {}'.format(', '.join(ROUTER_NAMES)))
