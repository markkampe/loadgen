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

To use the zombiemaster console

  (1) decide how many zombie servers you need, and do the above loadzombie
      server configuration on each.

	The typical reason to use a large number of loadzombie servers is that
	one client/initiator cannot generate enough traffic to seriously load
	a storage (e.g. NAS or iSCSI) server.

 (2) figure out what parameters you want to use for the load generation.

	You can read the loadgen manual section and try tests with different
	combinations of parameters until you get the test you want.

	A few of these (e.g. bsize, length, rate, update) can be specified
	as horde parameters.  Others (e.g. target) must be specified per
	zombie.

(3) figure out how you want to display this information on the console.

 	choose a title for the console window ("title" parameter)
	choose a per-zombie throughput scale ("wire" parameter)
	choose a whole horde throuhput scale ("scale" parameter)

 (4) create a horde configuration file

 	start with the sample file in java_src

	set the horde parameters with PARM lines

	crate a ZOMBIE line for each zombie, specifying
		tag= the unique identifier for this zombie
		zombie= the host name or IP address
		port= the port on which loadzombie is listening
		all per zombie configuration parameters

 (5) start up the zombie master

 	java -jar ZombieMaster.jar xxx name-of-horde-configuration-file

 	you might want to specify --DEBUG and capture stdout in a file

 (6) set any other parameters on the console

 	e.g. operation, size

 (7) click the start button to start all of the zombies

 (8) click the whole-horde or per-zombie +/- buttons to change the number
     of active threads.

 (9) click the stop button to stop all of the zombies

 (10) click the exit button on the ZombieMaster window to shut it down

