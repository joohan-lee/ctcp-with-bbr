# Note
- **When you test ping or traceroute, do it on router/host bash. Or if you are sending ping from a router, you can use quagga CLI.**
- vtysh is for configuring routers not hosts. So, on router bash, you can run it, but on host bash, you cannot.
- When you type wrong configuration, use `no` command to remove the configuration.
    - For example, to remove `ip address 4.109.0.2/24`, type `no ip address 4.109.0.2/24`
- 현재 config 확인하는 법: show run
- router마다 configuration 모두 끝나면 router별로 quagga CLI에서 `write file` 하여 config 파일 저장. 
- mininet 끄는 법 : (Ctrl+A) -> (Ctrl+D)

- Make sure to run `load_configs.py` and `config_i2_hosts.py` before configuring BGP.(mininet만 실행 후 바로 설정하면 router 별 interface, link 정보 등 모두 configuration 안 되어 있음.)
- router 설정 여는 방법:
    - cd lab2
    - ./go_to.sh SEAT
    - (router bash) vtysh
- Check the state of BGP sessions using the following command:
    - router_name# show ip bgp summary
- Check the routes learned from other BGP neighbors using the following command:
    - router_name# show ip route bgp

- root에서 configs(router별 .sav파일들) 복사하기.
    - cp -r /root/configs/ ./configs_multiAS
# 1. Configure BGP router
## 1-1. Configure AS Number of router for BGP
- command: **router bgp _asn_**
- Enable a BGP protocol process with the specified asn. After this statement you can input any BGP Commands.
- _asn_ is AS number.

### Sample
- router_name# conf t
- router_name(config)# router bgp 2

## 1-2. Configure router-ID
- BGP command: **bgp router-id _A.B.C.D_**
- This command specifies the router-ID(=A.B.C.D).
- For BGP router-id(A.B.C.D), Use IP Address of the interface which connects to the host.
### Sample
- router_name(config)# bgp router-id <ip_address>

# 2. Configure BGP Peer
## 2-1. Defining Peer
- BGP command: **neighbor _peer_ remote-as _asn_**
- Creates a new neighbor whose remote-as is asn.
- _peer_ can be an IPv4 address or an IPv6 address of neighbor(peer).
- _asn_ is AS number of _peer_.

### Sample1 from quagga document
- router bgp 1
- neighbor 10.0.0.1 remote-as 2
    - In this case my router, in AS-1, is trying to peer with AS-2 at 10.0.0.1. This command must be the first command used when configuring a neighbor. If the remote-as is not specified, bgpd will complain like this:
        - can’t find neighbor 10.0.0.1
### Sample2 from lab-manual
- router_name(config)# router bgp 2
- router_name(config-router)# neighbor 150.0.0.1 remote-as 15
- router_name(config-router)# neighbor 2.0.0.2 remote-as 2
    - By default, whenever the remote-as is different from the local number (here, 2), the BGP session is configured as an external one (i.e., an eBGP is established). In contrast, when the remote-as is equivalent to the local one, the BGP session is configured as an internal one. Here, the first session is an eBGP session, established with a router in another AS (150.0.0.1, AS NUMBER: 15), while the second one is internal session (iBGP), established with a router within your AS (2.0.0.2, AS NUMBER: 2).

## 2-2. Configure BGP loopback address (host interface's IP addr)
- BGP command: **neighbor _peer_ update-source <ifname|address>**
- _peer_ can be router's loopback ip address and ifname can be 'host' in our case.
- Specify the IPv4 source address to use for the BGP session to this neighbour, may be specified as either an IPv4 address directly or as an interface name (in which case the zebra daemon MUST be running in order for bgpd to be able to retrieve interface state).
- `neighbor 150.0.0.1 remote-as 15` command를 통해 peer를 define할 때 loopback address를 썼다면, 현재 router가 neighbor router에게 데이터를 보낼 때 neighbor router의 여러 interface 중 loopback address를 사용하여 데이터를 보낼 수 있도록 위 커맨드를 꼭 사용해야 함.

### Sample from quagga document
- router bgp 64555
    - neighbor foo update-source 192.168.0.1
    - neighbor bar update-source lo0
### Sample from lab-manual
- After configuring "neighbor <ip> remote-as <AS>", you also need to add the following command:
- "neighbor \<peer-group-name> update-source host (loopback address)" 
- (For more, see [this](https://www.cisco.com/c/en/us/support/docs/ip/border-gateway-protocol-bgp/13751-23.html))

## 2-3. BGP Peer command
- BGP command: **neighbor _peer_ next-hop-self [all]**
- This command specifies an announced route’s nexthop as being equivalent to the address of the bgp router if it is learned via eBGP. If the optional keyword all is specified the modifiation is done also for routes learned via iBGP.
- _peer_ can be an IPv4 address or an IPv6 address of neighbor(peer).
- 이 command는 eBGP로부터 배운 route를 next hop에 전달하기 위해 BGP 속성 중 next-hop을 정해주기 위해 하는 것 같다.

### Sample
- router_name(config-router)# neighbor <ip_address> next-hop-self


# (Optional) 3. BGP Network
## 3-1. BGP route
- BGP command: **network _A.B.C.D/M_**
- This command adds the announcement network.
### Sample from quagga document
- router bgp 1
    - network 10.0.0.0/8
- Should be run after BGP sessions are established which conducted in section 1,2.
- This configuration example says that network 10.0.0.0/8 will be announced to all neighbors. Some vendors’ routers don’t advertise routes if they aren’t present in their IGP routing tables; bgpd doesn’t care about IGP routes when announcing its routes.
### Sample from lab-manual
- router_name(config-router)# network 10.104.0.0/24

# (Optional) 4. BGP peer-group
- peer-group은 BGP router 별로 종속된다. 즉, 한 router에서 만들고 configure한 peer group을 다른 router와 공유되지 않는다. router 별로 여러개의 peer group을 가질 수 있다.
## 4-1. Create BGP Peer Group
- [BGP] command: **neighbor _word_ peer-group**
- This command defines a new peer group.

## 4-2. Configure AS number of peer group
- [BGP] command: **neighbor _my_peer_group_ remote-as <REMOTE_AS_NUMBER>**

## 4-3. Add peer into peer-group
- [BGP] command: **neighbor _peer_ peer-group _word_**
- This command bind specific peer to peer group word.

## 4-4. Configure Common Attributes
- 예시: neighbor i2_seat update-source host (i2_seat==peer group 이름, configure interface)

## Sample
- router bgp <AS_NUMBER>
- neighbor _my_peer_group_ peer-group
- router bgp <AS_NUMBER>
- neighbor <NEIGHBOR_IP> peer-group _my_peer_group_


# Router 별 BGP Router commands

## --- peer group 쓴 버전

### SEAT
- router_name# conf t
#### configure BGP router
- router_name(config)# router bgp 4
- router_name(config-router)# bgp router-id 4.109.0.2
#### define a new peer group
- router_name(config-router)# neighbor i2_seat peer-group
#### configure AS number of peer group(define peer)
- neighbor i2_seat remote-as 4
#### Add SEAT to peer group
- router_name(config-router)# neighbor 4.108.0.2 peer-group i2_seat
- router_name(config-router)# neighbor 4.107.0.2 peer-group i2_seat
- router_name(config-router)# neighbor 4.106.0.2 peer-group i2_seat
- router_name(config-router)# neighbor 4.105.0.2 peer-group i2_seat
- router_name(config-router)# neighbor 4.104.0.2 peer-group i2_seat
- router_name(config-router)# neighbor 4.103.0.2 peer-group i2_seat
- router_name(config-router)# neighbor 4.102.0.2 peer-group i2_seat
- router_name(config-router)# neighbor 4.101.0.2 peer-group i2_seat
#### define peer of eBGP
- router_name(config-router)# neighbor 5.0.1.2 remote-as 5 //west(eBGP)
#### configure peer interface
- router_name(config-router)# neighbor i2_seat update-source host
- router_name(config-router)# neighbor 5.0.1.2 update-source seat //west(eBGP)
#### configure next-hop-self
- router_name(config-router)# neighbor i2_seat next-hop-self
- router_name(config-router)# neighbor 5.0.1.2 next-hop-self //west(eBGP)
# advertise prefixes using _network_ command
- router_name(config-router)# network 5.0.0.0/8

### SALT
- router_name# conf t
- router_name(config)# router bgp 4
- router_name(config-router)# bgp router-id 4.107.0.2
- neighbor i2_salt peer-group
- neighbor i2_salt remote-as 4
- neighbor 4.109.0.2 peer-group i2_salt
- neighbor 4.108.0.2 peer-group i2_salt
- neighbor 4.106.0.2 peer-group i2_salt
- neighbor 4.105.0.2 peer-group i2_salt
- neighbor 4.104.0.2 peer-group i2_salt
- neighbor 4.103.0.2 peer-group i2_salt
- neighbor 4.102.0.2 peer-group i2_salt
- neighbor 4.101.0.2 peer-group i2_salt
- neighbor i2_salt update-source host
- neighbor i2_salt next-hop-self

### LOSA
- router_name# conf t
- router_name(config)# router bgp 4
- router_name(config-router)# bgp router-id 4.108.0.2
- neighbor i2_losa peer-group
- neighbor i2_losa remote-as 4
- neighbor 4.109.0.2 peer-group i2_losa
- neighbor 4.107.0.2 peer-group i2_losa
- neighbor 4.106.0.2 peer-group i2_losa
- neighbor 4.105.0.2 peer-group i2_losa
- neighbor 4.104.0.2 peer-group i2_losa
- neighbor 4.103.0.2 peer-group i2_losa
- neighbor 4.102.0.2 peer-group i2_losa
- neighbor 4.101.0.2 peer-group i2_losa
- neighbor i2_losa update-source host
- neighbor i2_losa next-hop-self

### KANS
- router_name# conf t
- router_name(config)# router bgp 4
- router_name(config-router)# bgp router-id 4.105.0.2
- neighbor i2_kans peer-group
- neighbor i2_kans remote-as 4
- neighbor i2_kans update-source host
- neighbor i2_kans next-hop-self
- neighbor 4.109.0.2 peer-group i2_kans
- neighbor 4.108.0.2 peer-group i2_kans
- neighbor 4.107.0.2 peer-group i2_kans
- neighbor 4.106.0.2 peer-group i2_kans
- neighbor 4.104.0.2 peer-group i2_kans
- neighbor 4.103.0.2 peer-group i2_kans
- neighbor 4.102.0.2 peer-group i2_kans
- neighbor 4.101.0.2 peer-group i2_kans

### HOUS
- router_name# conf t
- router_name(config)# router bgp 4
- router_name(config-router)# bgp router-id 4.106.0.2
- neighbor i2_hous peer-group
- neighbor i2_hous remote-as 4
- neighbor i2_hous update-source host
- neighbor i2_hous next-hop-self
- neighbor 4.109.0.2 peer-group i2_hous
- neighbor 4.108.0.2 peer-group i2_hous
- neighbor 4.107.0.2 peer-group i2_hous
- neighbor 4.105.0.2 peer-group i2_hous
- neighbor 4.104.0.2 peer-group i2_hous
- neighbor 4.103.0.2 peer-group i2_hous
- neighbor 4.102.0.2 peer-group i2_hous
- neighbor 4.101.0.2 peer-group i2_hous

### CHIC
- router_name# conf t
- router_name(config)# router bgp 4
- router_name(config-router)# bgp router-id 4.102.0.2
- neighbor i2_chic peer-group
- neighbor i2_chic remote-as 4
- neighbor i2_chic update-source host
- neighbor i2_chic next-hop-self
- neighbor 4.109.0.2 peer-group i2_chic
- neighbor 4.108.0.2 peer-group i2_chic
- neighbor 4.107.0.2 peer-group i2_chic
- neighbor 4.106.0.2 peer-group i2_chic
- neighbor 4.105.0.2 peer-group i2_chic
- neighbor 4.104.0.2 peer-group i2_chic
- neighbor 4.103.0.2 peer-group i2_chic
- neighbor 4.101.0.2 peer-group i2_chic

### ATLA
- router_name# conf t
- router_name(config)# router bgp 4
- router_name(config-router)# bgp router-id 4.104.0.2
- neighbor i2_atla peer-group
- neighbor i2_atla remote-as 4
- neighbor i2_atla update-source host
- neighbor i2_atla next-hop-self
- neighbor 4.109.0.2 peer-group i2_atla
- neighbor 4.108.0.2 peer-group i2_atla
- neighbor 4.107.0.2 peer-group i2_atla
- neighbor 4.106.0.2 peer-group i2_atla
- neighbor 4.105.0.2 peer-group i2_atla
- neighbor 4.103.0.2 peer-group i2_atla
- neighbor 4.102.0.2 peer-group i2_atla
- neighbor 4.101.0.2 peer-group i2_atla

### WASH
- router_name# conf t
- router_name(config)# router bgp 4
- router_name(config-router)# bgp router-id 4.103.0.2
- neighbor i2_wash peer-group
- neighbor i2_wash remote-as 4
- neighbor i2_wash update-source host
- neighbor i2_wash next-hop-self
- neighbor 4.109.0.2 peer-group i2_wash
- neighbor 4.108.0.2 peer-group i2_wash
- neighbor 4.107.0.2 peer-group i2_wash
- neighbor 4.106.0.2 peer-group i2_wash
- neighbor 4.105.0.2 peer-group i2_wash
- neighbor 4.104.0.2 peer-group i2_wash
- neighbor 4.102.0.2 peer-group i2_wash
- neighbor 4.101.0.2 peer-group i2_wash

### NEWY
- router_name# conf t
- router_name(config)# router bgp 4
- router_name(config-router)# bgp router-id 4.101.0.2
- neighbor i2_newy peer-group
- neighbor i2_newy remote-as 4
- neighbor i2_newy update-source host
- neighbor i2_newy next-hop-self
- neighbor 4.109.0.2 peer-group i2_newy
- neighbor 4.108.0.2 peer-group i2_newy
- neighbor 4.107.0.2 peer-group i2_newy
- neighbor 4.106.0.2 peer-group i2_newy
- neighbor 4.105.0.2 peer-group i2_newy
- neighbor 4.104.0.2 peer-group i2_newy
- neighbor 4.103.0.2 peer-group i2_newy
- neighbor 4.102.0.2 peer-group i2_newy
#### define peer of eBGP
- neighbor 6.0.1.2 remote-as 6 //east(eBGP)
#### configure peer interface
- neighbor 6.0.1.2 update-source newy //east(eBGP)
#### configure next-hop-self
- neighbor 6.0.1.2 next-hop-self //east(eBGP)
# advertise prefixes using _network_ command
- router_name(config-router)# network 6.0.0.0/16


### 여기부터는 internet2가 아닌 WEST, EAST router들
### west
- router_name# conf t
- router_name(config)# router bgp 5
- router_name(config-router)# bgp router-id 5.0.1.2
#### define eBGP peer
- neighbor 4.109.0.2 remote-as 4
<!-- - neighbor 4.109.0.2 update-source host -->
<!-- #### iBGP
- neighbor 5.0.2.2 remote-as 5
- neighbor 5.0.2.2 update-source eth2
- neighbor 5.0.2.2 next-hop-self -->
# advertise prefixes using _network_ command
- router_name(config-router)# network 4.0.0.0/6
# static route commands
- router_name(config)# ip route 5.1.1.0/24 5.0.2.2 [Command]
- Should run the above command because sr<->west does not communicate to each other


### east
- router_name# conf t
- router_name(config)# router bgp 6
- router_name(config-router)# bgp router-id 6.0.1.2
#### define eBGP peer
- neighbor 4.101.0.2 remote-as 4
<!-- - neighbor 4.101.0.2 update-source host -->


## --- peer group 안 쓴 버전

### LOSA
- router_name# conf t
- router_name(config)# router bgp 4
- router_name(config-router)# bgp router-id 4.108.0.2
- neighbor 4.109.0.2 remote-as 4
- neighbor 4.107.0.2 remote-as 4
- neighbor 4.106.0.2 remote-as 4
- neighbor 4.109.0.2 update-source host
- neighbor 4.107.0.2 update-source host
- neighbor 4.106.0.2 update-source host
- neighbor 4.109.0.2 next-hop-self
- neighbor 4.107.0.2 next-hop-self
- neighbor 4.106.0.2 next-hop-self

여기 KANS 부터 세팅.
### KANS
- router_name# conf t
- router_name(config)# router bgp 4
- router_name(config-router)# bgp router-id 4.105.0.2
- neighbor 4.109.0.2 remote-as 4
- neighbor 4.102.0.2 remote-as 4
- neighbor 4.106.0.2 remote-as 4
- neighbor 4.109.0.2 update-source host
- neighbor 4.102.0.2 update-source host
- neighbor 4.106.0.2 update-source host
- neighbor 4.109.0.2 next-hop-self
- neighbor 4.102.0.2 next-hop-self
- neighbor 4.106.0.2 next-hop-self