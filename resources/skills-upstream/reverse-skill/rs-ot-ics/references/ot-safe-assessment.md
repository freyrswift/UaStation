# OT Security Assessment Safety Checklist

1. Authorized scope and emergency contacts  
2. Whether active probing / write operations are allowed (default no)  
3. Maintenance windows and rollback plans  
4. Traffic mirroring preferred over port scanning  
5. On a high-severity finding, stop expanding immediately and report  
6. Report distinguishes: remotely exploitable vs requires physical access  

Common protocol ports (for identification, not an exploitation manual): Modbus/TCP 502, S7comm 102, EtherNet/IP 44818, DNP3 20000.
