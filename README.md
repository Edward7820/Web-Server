# Web server

## Function
 - 註冊使用者帳號密碼
 - 用帳號密碼登入 (登入後server會配發一個sessionId作為Cookie)
 - 用你的帳號在留言板上發表留言
 - 登出系統 (登出後server會刪除該sessionId)

## Environment
Ubuntu 20.04

## Compile and execution
Change the 17th line of server.c to the port number you want to use.
Change the 19th line of server.c to the path of python3 on your computer.
Then, execute the command 
```shell
touch users.json
touch messages.json
gcc server.c -o server
./server
```