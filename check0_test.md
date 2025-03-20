### 2.1 获取一个网站
telnet cs144.keithw.org http
GET /hello HTTP/1.1
HOST: cs144.keithw.org
Connection: close

<!-- 尝试获取多层内容 -->
http://httpbin.org/forms/post

telnet httpbin.org http
GET /forms/post HTTP/1.1    // 从第三个/开始之后的内容
HOST: httpbin.org           // http://和第三个/之间的内容
Connection: close           // 回车两次



<!-- 格式化代码 -->
cmake --build build --target format 