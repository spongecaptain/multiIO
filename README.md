# README

multiIO server example in C.

Step1: Build

```bash
# take select_test.c as example
gcc -o select select_test.c server_socket_init.c
```

Step2: Run Server

```bash
# take select as example
./select
```

Step3: Run Client by telnet

```bash
telnet 127.0.0.1 5110
# then  you can input some string and get echo string
```

