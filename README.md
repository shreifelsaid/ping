# ping

A program used to test a host on an Internet Protocol (IP) network

Shreif Abdallah
<selsaid@u.rochester.edu>

- Compile Instructions :-


```
    gcc ping.c -o myping
```


- Running Instructions :-

```
    sudo ./myping <host name or ip address>
```

- Extra credit :-


    TTL (Time to live) can be set as an optional command line argument, it is by defualt set to 54.

```
    sudo ./myping <host name or ip address> <TTL>
```


Please note that since ping relies on raw sockets, the code needs to be ran with sudo. 
