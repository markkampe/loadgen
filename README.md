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

  (2) make sure that xinetd is installed

	make sure that xinetd is running

	    # service --status-all

	It may not yet be installed.

	    CentOS:
		# yum install xinetd
		# chkconfig --level 345 xinetd on

  	    Ubuntu:
	    	# apt-get install xinetd

  (3) configure the loadzombie network service:

	edit "loadzombie" configuration file to choose a port.

	copy "loadzombie" configuration file to /etc/xinetd.d

		# cp loadzombie /etc/xinetd.d

	tell xinetd to rescan
	
		# service xinetd restart

  (4) test the loadzombie service:

	% telnet <node> <port>

		for   <node> on which the service is configured (local = 127.0.0.1)
		      <port> on which the service is configured (default = 8081)

		It should greet you with a loadzombie prompt.
		If you enter 'x' followed by a newline, it 
		will shut down and close the port.
