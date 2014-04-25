TimeSwitch4ModemRouter
======================

## 1、背景介绍
一般家庭modem和路由器都是24小时开机，这种设计很不环保。对于大部分人晚上睡觉和上班时间都不需要网络可以关闭。本设计就是在modem和路由器的电源线上加入定时开关，实现电源的自动开启和关闭。

![appearance](https://github.com/apeng2012/TimeSwitch4ModemRouter.git)

采用NuTiny-SDK-NUC122开发板。引出电路板的PD0、PD1和PD2 IO口控制PMOS开关从而控制电源开关。串口1作为与电脑相连的通讯接口用来设置定时时间。上位机采用python语言与电路板交互。

![Circuit](http://www.nuvoton.com/hq/enu/ProductAndSales/ProductLines/MicrocontrollerApplicationIC/ARMMicrocontroller/ARMCortexTMM0/PublishingImages/NuTiny-SDK-NUC120.jpg)

## 2、基本操作方法

将电路板串口1连接到电脑的一个串口上（电脑端串口需要电平转换到5V电平）。按电路板复位键后，运行

'''shell
python timeswitch.py 串口号
'''

一切正常会显示设置时间。上一次设置的开关机时间。
最后显示 "Please set switch time in SwitchTime.xml"
你可以打开SwitchTime.xml文件修改开关机时间段(下面是我的设置)，并保存。

'''xml
<switchTime>
	<sunday>
		<item>06:30-23:30</item>
	</sunday>
	<monday>
		<item>06:30-09:00</item>
		<item>18:00-23:30</item>
	</monday>
	<tuesday>
		<item>06:30-09:00</item>
		<item>18:00-23:30</item>
	</tuesday>
	<wednesday>
		<item>06:30-09:00</item>
		<item>18:00-23:30</item>
	</wednesday>
	<thursday>
		<item>06:30-09:00</item>
		<item>18:00-23:30</item>
	</thursday>
	<friday>
		<item>06:30-09:00</item>
		<item>18:00-23:30</item>
	</friday>
	<saturday>
		<item>06:30-23:30</item>
	</saturday>
</switchTime>
'''

返回刚才的窗口按回车键，将刚才的设置写入电路板。复位电路就可以正常工作了