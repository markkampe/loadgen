Loadgen is an file traffic generator that can change its offerred
load (quickly) in response to numeric input (how many threads)
on stdin, and that reports actual achieved throughput regularly
on stdout.  

It is actually designed to be driven (as a horde of zombies) by 
a master program that manages load and collects and displays 
throughput data.  Thus it may be desirable to turn lots of
client machines into loadzombies that can be called into 
service when we need their help.

To turn an ordinary system into a loadzombie:

  (1) install the appropriate (32/64) loadgen in /usr/bin.

  (2) configure the loadzombie network service:

    CentOS
	copy "loadzombie" to /etc/xinetd.d

		This file sets up loadgen to serve port 8080.
		I chose this because it is passed through our 
		fire-walls and not used by our client systems.
		You can choose any port you want.

	make sure that xinetd is running

		# chkconfig --level 345 xinetd on

	It may not yet be installed.  To install:

		# yum install xinetd
		# chkconfig --level 345 xinetd on
		# service xinetd restart

	confirm that loadzombie is a registered service
	
		# chkconfig --list

    Ubuntu

  (3) test the loadzombie service:

	% telnet <node> <port>

		for   <node> on which the service is configured
		      <port> on which the service is configured

		It should greet you with a loadzombie prompt.
		If you enter 'x' followed by a newline, it 
		will shut down and close the port.
