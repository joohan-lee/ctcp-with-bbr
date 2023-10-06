# peer-group 사용법(예시)
In Quagga, which is an open-source routing software suite, the `peer-group` command is used to group BGP peers together with similar configurations. This can simplify the configuration process, especially when you have multiple BGP peers that share common attributes. Here's a basic guide on how to use the `peer-group` command in Quagga:

1. **Access Configuration Mode**:

   Access the global configuration mode in Quagga. This can typically be done by entering the following command:

   ```
   configure terminal
   ```

2. **Create a Peer Group**:

   Use the `peer-group` command to create a new peer group. You'll need to provide a name for the group. For example, let's create a peer group named `my_peer_group`:

   ```
   router bgp <AS_NUMBER>
   neighbor my_peer_group peer-group
   ```

3. **Configure Common Attributes**:

   Once the peer group is created, you can configure attributes that will apply to all members of the group. For example, if you want to set the remote AS number for all peers in `my_peer_group`, you can use:

   ```
   router bgp <AS_NUMBER>
   neighbor my_peer_group remote-as <REMOTE_AS_NUMBER>
   ```

4. **Add Peers to the Group**:

   Now you can add individual BGP neighbors to the peer group. When a neighbor is added to a group, it inherits the attributes configured for that group.

   ```
   router bgp <AS_NUMBER>
   neighbor <NEIGHBOR_IP> peer-group my_peer_group
   ```

   The above command adds the BGP neighbor with the IP address `<NEIGHBOR_IP>` to the peer group `my_peer_group`.

5. **Configure Individual Attributes (Optional)**:

   You can still configure attributes on individual neighbors that will override the attributes inherited from the peer group.

   ```
   router bgp <AS_NUMBER>
   neighbor <NEIGHBOR_IP> remote-as <REMOTE_AS_NUMBER>
   ```

6. **Verify Configuration**:

   You can use the `show running-config` command to verify that the peer group and its associated configurations have been properly set up.

7. **Save Configuration (if needed)**:

   If you're satisfied with the configuration, don't forget to save it to ensure it persists across reboots.

   ```
   write memory
   ```

Please replace `<AS_NUMBER>`, `<REMOTE_AS_NUMBER>`, and `<NEIGHBOR_IP>` with your actual values.

Keep in mind that using peer groups can greatly simplify BGP configuration, especially in environments with many BGP peers that share common attributes. However, be cautious with this approach, as individual configurations may override those inherited from the peer group, and this could lead to unexpected behavior if not managed properly.


# peer-group 사용 이유
A peer group in BGP (Border Gateway Protocol) is a way to group together multiple BGP peers that share common configuration parameters. These peers can be in the same or different Autonomous Systems (ASes), and they can be routers from different vendors.

Here are a few key points to consider about BGP peer groups:

1. **Same AS or Different AS**:
   
   - **Same AS**: The routers in a peer group can belong to the same AS. This is common in large networks where multiple routers within the same AS have similar BGP configurations.
   
   - **Different AS**: Routers in a peer group can also belong to different ASes. This can be useful when you have multiple routers that need to have similar BGP configurations but are in different ASes.

2. **Common Attributes**:

   - All routers in a peer group share common BGP attributes and configurations. This can include things like the remote AS number, update source, next-hop self, and others.

3. **Configuration Simplification**:

   - Peer groups help to simplify BGP configurations, especially in networks with a large number of BGP peers. Instead of configuring each peer individually, you can apply common settings to the entire peer group.

4. **Individual Overrides**:

   - While peer groups provide a way to apply common configurations, individual peers within a group can still have unique attributes that override the settings inherited from the group.

5. **Advantages**:

   - Peer groups can help reduce configuration overhead and potential errors. They also make it easier to scale BGP configurations in large networks.

6. **Disadvantages**:

   - Using peer groups may lead to less granular control over individual peers. If you need fine-grained control over each peer, you may choose not to use peer groups.

In summary, routers in a peer group can be in the same AS or different ASes. The key factor is that they share common configuration parameters, which makes managing BGP configurations more efficient. This is particularly useful in networks where you have a large number of peers with similar attributes.

### 사용 예시
실례로 아래와 같은 command를 입력해야할 때,
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

4.109.0.2, 4.107.0.2, 4.106.0.2 세 개를 peer-group으로 묶으면 편리할 듯 하다.