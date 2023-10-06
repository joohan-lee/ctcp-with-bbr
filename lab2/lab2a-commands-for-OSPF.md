# Note
- **When you test ping or traceroute, do it on router/host bash. Or if you are sending ping from a router, you can use quagga CLI.**
- vtysh is for configuring routers not hosts. So, on router bash, you can run it, but on host bash, you cannot.
- When you type wrong configuration, use ```no``` command to remove the configuration.
    - For example, to remove ```ip address 4.109.0.2/24```, type ```no ip address 4.109.0.2/24```
- config 파일 읽는 법: show run
- router마다 configuration 모두 끝나면 router별로 quagga CLI에서 ```write file``` 하여 config 파일 저장. 

# 1. Configure IP addr of Routers' interfaces
## Sample(SEAT router, host interface)
- cd lab2
- ./go_to.sh SEAT
- (router bash) vtysh
- router_name# conf t
- router_name(config)# interface host
- router_name(config-if)# ip address 4.109.0.2/24
- ```show interface``` to check if interfaces were properly configured.
- (exit to original bash)
## SEAT
- router_name(config)# interface host
- router_name(config-if)# ip address 4.109.0.2/24
- router_name(config)# interface salt
- router_name(config-if)# ip address 4.0.12.2/24
- router_name(config)# interface losa
- router_name(config-if)# ip address 4.0.13.2/24
- router_name(config)# interface west
- router_name(config-if)# ip address 5.0.1.1/24

## LOSA
- router_name(config)# ```interface host```
- router_name(config-if)# ```ip address 4.108.0.2/24```
- interface seat
- ip address 4.0.13.1/24
- interface salt
- ip address 4.0.11.2/24
- interface hous
- ip address 4.0.10.2/24

## SALT
- interface host
- ip address 4.107.0.2/24
- interface kans
- ip address 4.0.9.2/24
- interface losa
- ip address 4.0.11.1/24
- interface seat
- ip address 4.0.12.1/24

## KANS
- interface host
- ip address 4.105.0.2/24
- interface chic
- ip address 4.0.6.2/24
- interface hous
- ip address 4.0.8.1/24
- interface salt
- ip address 4.0.9.1/24

## HOUS
- interface host
- ip address 4.106.0.2/24
- interface losa
- ip address 4.0.10.1/24
- interface kans
- ip address 4.0.8.2/24
- interface atla
- ip address 4.0.7.2/24

## CHIC
- interface host
- ip address 4.102.0.2/24
- interface newy
- ip address 4.0.1.2/24
- interface wash
- ip address 4.0.2.2/24
- interface atla
- ip address 4.0.3.2/24
- interface kans
- ip address 4.0.6.1/24

## ATLA
- interface host
- ip address 4.104.0.2/24
- interface hous
- ip address 4.0.7.1/24
- interface chic
- ip address 4.0.3.1/24
- interface wash
- ip address 4.0.5.2/24

## NEWY
- interface host
- ip address 4.101.0.2/24
- interface wash
- ip address 4.0.4.1/24
- interface chic
- ip address 4.0.1.1/24
- interface east
- ip address 6.0.1.1/24

## WASH
- interface host
- ip address 4.103.0.2/24
- interface atla
- ip address 4.0.5.1/24
- interface chic
- ip address 4.0.2.1/24
- interface newy
- ip address 4.0.4.2/24
## west
- interface seat
- ip address 4.109.0.2/24
- interface sr
- ip address 5.0.2.1/24
## east
- interface newy
- ip address 6.0.1.2/24
- interface server1
- ip address 6.0.2.1/24
- interface server2
- ip address 6.0.3.1/24

# 2. Configure OSPF
- OSPF routers flood IP routes over OSPF adjacencies.
## Sample(SEAT router)
- router_name# ```conf t```
- router_name(config)# ```router ospf```
- router_name(config-router)# ```network 4.109.0.0/24 area 0```
- router_name(config-router)# ```network 4.0.12.0/24 area 0```
- router_name(config-router)# ```network 4.0.13.0/24 area 0```
- router_name(config-router)# ```network 5.0.1.0/24 area 0```

## SEAT
- network 4.109.0.0/24 area 0
- network 4.0.12.0/24 area 0
- network 4.0.13.0/24 area 0
- network 5.0.1.0/24 area 0

## LOSA
- network 4.0.13.0/24 area 0
- network 4.0.11.0/24 area 0
- network 4.0.10.0/24 area 0
- network 4.108.0.0/24 area 0

## SALT
- network 4.107.0.0/24 area 0
- network 4.0.9.0/24 area 0
- network 4.0.11.0/24 area 0
- network 4.0.12.0/24 area 0

## KANS
- network 4.105.0.0/24 area 0
- network 4.0.6.0/24 area 0
- network 4.0.8.0/24 area 0
- network 4.0.9.0/24 area 0

## HOUS
- network 4.106.0.0/24 area 0
- network 4.0.10.0/24 area 0
- network 4.0.8.0/24 area 0
- network 4.0.7.0/24 area 0

## CHIC
- network 4.102.0.0/24 area 0
- network 4.0.1.0/24 area 0
- network 4.0.2.0/24 area 0
- network 4.0.3.0/24 area 0
- network 4.0.6.0/24 area 0

## ATLA
- network 4.104.0.0/24 area 0
- network 4.0.7.0/24 area 0
- network 4.0.3.0/24 area 0
- network 4.0.5.0/24 area 0

## NEWY
- network 4.101.0.0/24 area 0
- network 4.0.4.0/24 area 0
- network 4.0.1.0/24 area 0
- network 6.0.1.0/24 area 0

## WASH
- network 4.103.0.0/24 area 0
- network 4.0.5.0/24 area 0
- network 4.0.2.0/24 area 0
- network 4.0.4.0/24 area 0

## west
- network 5.0.1.0/24 area 0
- network 5.0.2.0/24 area 0
## east
- network 6.0.1.0/24 area 0
- network 6.0.2.0/24 area 0
- network 6.0.3.0/24 area 0

# 3. Configure the weight of link for OSPF
- each interface connected to another router has ospf cost
## Sample(SEAT router)
- router_name# ```conf t```
- router_name(config)# ```interface salt```
- router_name(config-if)# ```ip ospf cost 913```

## SEAT
- interface salt
- ip ospf cost 913
- interface losa
- ip ospf cost 1342

## LOSA
- interface seat
- ip ospf cost 1342
- interface salt
- ip ospf cost 1303
- interface hous
- ip ospf cost 1705

## SALT
- interface kans
- ip ospf cost 1330
- interface losa
- ip ospf cost 1303
- interface seat
- ip ospf cost 913

## KANS
- interface chic
- ip ospf cost 690
- interface hous
- ip ospf cost 818
- interface salt
- ip ospf cost 1330

## HOUS
- interface losa
- ip ospf cost 1705
- interface kans
- ip ospf cost 818
- interface atla
- ip ospf cost 1385

## CHIC
- interface newy
- ip ospf cost 1000
- interface wash
- ip ospf cost 905
- interface atla
- ip ospf cost 1045
- interface kans
- ip ospf cost 690

## ATLA
- interface hous
- ip ospf cost 1385
- interface chic
- ip ospf cost 1045
- interface wash
- ip ospf cost 700

## NEWY
- interface wash
- ip ospf cost 277
- interface chic
- ip ospf cost 1000

## WASH
- interface atla
- ip ospf cost 700
- interface chic
- ip ospf cost 905
- interface newy
- ip ospf cost 277

# 4. Save configuration file
- ```write file``` after configuration at each router quagga CLI.

# 5. Configure Default Gateways of Hosts' interfaces
## Sample(SEAT-host default gateway)
- cd lab2
- sudo ./go_to.sh SEAT-host
- sudo ifconfig seat 4.109.0.1/24 up
- sudo route add default gw 4.109.0.2 seat
- (exit to original bash)


## SEAT-host
- sudo ifconfig seat 4.109.0.1/24 up
- sudo route add default gw 4.109.0.2 seat

## LOSA-host
- sudo ifconfig losa 4.108.0.1/24 up
- sudo route add default gw 4.108.0.2 losa

## SALT-host
- sudo ifconfig salt 4.107.0.1/24 up
- sudo route add default gw 4.107.0.2 salt

## KANS-host
- sudo ifconfig kans 4.105.0.1/24 up
- sudo route add default gw 4.105.0.2 kans

## HOUS-host
- sudo ifconfig hous 4.106.0.1/24 up
- sudo route add default gw 4.106.0.2 hous

## CHIC-host
- sudo ifconfig chic 4.102.0.1/24 up
- sudo route add default gw 4.102.0.2 chic

## ATLA-host
- sudo ifconfig atla 4.104.0.1/24 up
- sudo route add default gw 4.104.0.2 atla

## NEWY-host
- sudo ifconfig newy 4.101.0.1/24 up
- sudo route add default gw 4.101.0.2 newy

## WASH-host
- sudo ifconfig wash 4.103.0.1/24 up
- sudo route add default gw 4.103.0.2 wash
