设定出错率，达到累计的传包次数时，对包的校验和字段进行随机替换（这样确认时不会得到16位全1结果）；否则在重传时，还原数据包的校验和字段。主要代码如下：

![image-20221119183129677](C:\Users\18213\AppData\Roaming\Typora\typora-user-images\image-20221119183129677.png)

打印得到的日志：

- 客户端（发送方）

下图为制作错包，服务器不回应，超时重传的示例

![image-20221119183829172](C:\Users\18213\AppData\Roaming\Typora\typora-user-images\image-20221119183829172.png)



![image-20221119183626147](C:\Users\18213\AppData\Roaming\Typora\typora-user-images\image-20221119183626147.png)

- 服务器（接收方）

下图为服务器接收到错包且不回应的示例

![image-20221119183734068](C:\Users\18213\AppData\Roaming\Typora\typora-user-images\image-20221119183734068.png)

![image-20221119183914648](C:\Users\18213\AppData\Roaming\Typora\typora-user-images\image-20221119183914648.png)