import csv
import subprocess
import sys
import os

def read_csv(file_path):

    with open(file_path, 'r') as file:
        csv_reader = csv.DictReader(file)

        links = []
        
        for row in csv_reader:
            if '-host' in row['Node1'] or '-host' in row['Node2']:
                links.append(row)
    
    return links

def generate_cmds(links):
    cmds = []
    host_nodes = []
    for i in range(len(links)):
        link = links[i]
        if '-host' in link['Node2']:
            iface_host = link['Interface2']
            ip_host = link['Address2']
            ip_gw = link['Address1']
            host_nodes.append(link['Node2'])
            
        elif '-host' in link['Node1']:
            iface_host = link['Interface1']
            ip_host = link['Address1']
            ip_gw = link['Address2']
            host_nodes.append(link['Node1'])
        ip_host += '/24' # subnet mask
        cmd1 = "sudo ifconfig {} {} up".format(iface_host, ip_host)
        cmd2 = "sudo route add default gw {} {}".format(ip_gw, iface_host)

        cmds.append([cmd1, cmd2])

    return cmds, host_nodes

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

if __name__ == "__main__":
    # csv_file_names = ['asns.csv', 'hosts.csv', 'links.csv', 'routers.csv']
    csv_file = './netinfo/links.csv'
    
    
    # Net, Node1, Interface1, Address1, Node2, Interface2, Address2, Cost = read_csv(csv_file)
    links = read_csv(csv_file)

    cmds, host_nodes = generate_cmds(links)

    for host_node, cmd in zip(host_nodes, cmds):
        pid = get_pid_mx_host(host_node)
        cmd_str = '; '.join(cmd)
        cmd_str = '"' + cmd_str + '"'
        host_config_cmd = "exec sudo mxexec -a %s -b %s -k %s bash -c %s" % (pid, pid, pid, cmd_str)
        # print(host_config_cmd)
        try:
            # print('subprocess started')
            subprocess.check_call(host_config_cmd, shell=True)
            # print('subprocess ended')
        except subprocess.CalledProcessError as e:
            print("Error: %s" % e)

    

    

        