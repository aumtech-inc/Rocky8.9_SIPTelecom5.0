systemd / systemctl tips
========================

To successfully transfer Aumtech's log files to this system, the log_listen 
binary is to be scheduled as a systemd service.  

For all non-centos versions, the log_listen binary is located in /usr/local/bin
because the executable cannot be in a non-system directory (i.e. /home/arc/...).

1. A systemd service operates on 'service' configuration files in the /etc/systemd/system directory.
   The file /etc/systemd/system/log_listen.service was installed and enabled by the arcLOGS package.

2. To reload the service systemd configuration with the newly added service file with the command:
	# systemctl daemon-reload		

3. To start the service:
	# systemctl start log_listen

4. To query it:
	# systemctl status  log_listen

	You should see something like:
		[root@new235a system]# systemctl status log_listen
		AcLogListen.service - Aumtech's Log Listener service
		   Loaded: loaded (/etc/systemd/system/log_listen.service; disabled; vendor preset: disabled)
		   Active: active (running) since Thu 2020-07-15 15:45:03 EST; 
		 Main PID: 32322 (log_listen)
		   CGroup: /system.slice/log_listen.service
		           â2322 log_listen ISP/Global/Exec/log_listen LOG 30
		
		Jul 15 15:45:03 new235a systemd[1]: Started Aumtech's Log Listener service.
		
5. To stop it:
	# systemctl stop log_listen

6. To disable it:
	// stop the service
	# systemctl stop log_listen

	// disable it by renaming the it to file that does not end with 'service'
	# cd /etc/systemd/system
	# mv log_listen.service log_listen.service.noRun

	// reload the systemd configuration
	# systemctl daemon-reload
	
