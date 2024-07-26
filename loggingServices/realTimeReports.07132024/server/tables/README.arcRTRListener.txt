systemd / systemctl tips
========================

To successfully transfer Aumtech's CDR log files to this system in real time, the arcRTRListener
binary is to be scheduled as a systemd service.

For all non-centos versions, the arcRTRListener binary is located in /usr/local/bin
because the executable cannot be in a non-system directory (i.e. /home/arc/...).

1. A systemd service operates on 'service' configuration files in the /etc/systemd/system directory.
   The file /etc/systemd/system/arcRTRListener.service was installed and enabled by the arcLOGS package.

2. To reload the service systemd configuration with the newly added service file with the command:
	# systemctl daemon-reload		

3. To start the service:
	# systemctl start arcRTRListener

4. To query it:
	# systemctl status arcRTRListener

	You should see something like:
		[root@new235a system]# systemctl status arcRTRListener
		AcLogListen.service - Aumtech's Real-time Reports Listener service
		   Loaded: loaded (/etc/systemd/system/arcRTRListener.service; disabled; vendor preset: disabled)
		   Active: active (running) since Thu 2020-07-15 15:45:03 EST; 
		 Main PID: 32322 (log_listen)
		   CGroup: /system.slice/arcRTRListener.service
		           â2322 larcRTRListener ISP/Global/Exec/arcRTRListener LOG 30
		
		Jul 15 15:45:03 new235a systemd[1]: Started Aumtech's Aumtech's Real-time Reports Service.
		
5. To stop it:
	# systemctl stop arcRTRListener

6. To disable it:
	// stop the service
	# systemctl stop arcRTRListener

	// disable it by renaming the it to file that does not end with 'service'
	# cd /etc/systemd/system
	# mv arcRTRListener.service arcRTRListener.service.noRun

	// reload the systemd configuration
	# systemctl daemon-reload
	
