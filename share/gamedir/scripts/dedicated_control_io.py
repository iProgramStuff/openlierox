#!/usr/bin/python -u
# Dedicated Control handler script for OpenLieroX
# (http://openlierox.sourceforge.net)


# Needed for sleeping/pausing execution
import time
# Needed for directory access
import os
import sys
import threading

import dedicated_config  # Per-host config like admin password
cfg = dedicated_config # shortcut


## Receiving functions ##

# Non-blocking IO class
# You should use getSignal() function to get another signal, not read from stdin.
# getSignal() will return empty string if there is nothing to read.
# use pushSignal() if you want to have some signal later read by getSignal()


# This is just technical mumbojumbo, not really interresting for the common user (you)
# You can ignore this part: What's important is that this part catches EVERYTHING sent by OLX,
# puts it in it's buffer, and delivers it to anyone who asks by getResponse().

bufferedSignalsLock = threading.Lock()
bufferedSignals = []

# The thread to read from stdin
class GetStdinThread(threading.Thread):
	def __init__(self):
		threading.Thread.__init__(self)
		self.start()

	def run(self):
		global bufferedSignals, bufferedSignalsLock
		try:
			emptyLines = 0 # Hack to stdin being silently closed
			while True: # Killed by sys.exit(), that's hack but I'm too lazy to do correct thread exit
				line = sys.stdin.readline()
				if line.strip() != "":
					emptyLines = 0
					bufferedSignalsLock.acquire()
					bufferedSignals.append(line.strip())
					bufferedSignalsLock.release()
				else:
					emptyLines += 1
				if sys.stdin.closed or emptyLines > 100:
					raise Exception, "stdin closed"
		except Exception:
			sys.stderr.write("Broken Pipe -- exiting")
			# OLX terminated, stdin = broken pipe, we should terminate also
			sys.exit()

# Use this function to get signals, don't access bufferedSignals array directly
def getSignal():
	global bufferedSignals, bufferedSignalsLock
	ret = ""
	bufferedSignalsLock.acquire()
	if len(bufferedSignals) > 0:
		ret = bufferedSignals[0]
		bufferedSignals = bufferedSignals[1:]
	bufferedSignalsLock.release()
	#if ret != "": msg("Got signal \"%s\"" % ret)
	return ret

# Use this function to push signal into bufferedSignals array, don't access bufferedSignals array directly
def pushSignal(sig):
	global bufferedSignals, bufferedSignalsLock
	bufferedSignalsLock.acquire()
	bufferedSignals.append(sig)
	bufferedSignalsLock.release()

# Wait until specific signal, returns params of that signal (strips signal name), saves all extra signals.
def getResponse(cmd):
	ret = ""
	extraSignals = []
	resp = getSignal()
	while ret == "":
		if resp.find(cmd+" ") == 0:
			ret = resp[len(cmd)+1:] # Strip cmd string and push the rest to return-value
		else:
			if resp != "":
				extraSignals.append(resp)
			else:
				time.sleep(0.01)
		if ret == "":
			resp = getSignal()
	for s in extraSignals:
		pushSignal(s)
	return ret

# This function is indended to parse something like:
# "newworm 1 Player1" -> we call "getwormlist" and getResponseList("wormlistinfo")
# "gamestarted" is arrived when getResponseList("wormlistinfo") called, then worm list info arrives:
# "wormlistinfo 0 [CPU] Kamikazee!"
# "wormlistinfo 1 Player1"
# "endlist"
# It will return array [ "0 [CPU] Kamikazee!", "1 Player1" ] and push back "gamestarted" signal so we will parse it next.
def getResponseList(cmd):
	ret = []
	extraSignals = []
	resp = getSignal()
	while resp != "endlist":
		if resp.find(cmd+" ") == 0:
			ret.append( resp[len(cmd)+1:] ) # Strip cmd string and push the rest to return-value
		else:
			if resp != "":
				extraSignals.append(resp)
			else:
				time.sleep(0.01)
		resp = getSignal()
	for s in extraSignals:
		pushSignal(s)
	return ret

## Sending functions ##

# Set a server variable
def setvar(what, data):
	print "setvar %s %s" % (str(what), str(data))

# Use this to make the server quit
def Quit():
	print "quit"

# Use this to force the server into lobby - it will kick all connected worms and restart the server
# TODO: Why do we send this with startlobby? Server doesn't catch it, + it's bad style.
# Use addWorm() instead
#def startLobby(localWorm = "[CPU] Kamikazee!"):
#	print "startlobby " + localWorm
def startLobby(port):
	global sentStartGame
	if port:
		print "startlobby " + str(port)
	else:
		print "startlobby"
	sentStartGame = False

# Force the server into starting the game (weapon selections screen)
def startGame():
	global sentStartGame
	print "startgame"
	sentStartGame = True

# Use this to force the server into lobby - it will abort current game but won't kick connected worms
def gotoLobby():
	global sentStartGame
	print "gotolobby"
	sentStartGame = False

# Not implemented yet in OLX
def addBot(name):
	print "addbot " + str(name)

# Suicides all local bots
def killBots():
	print "killbots"

# Both kick and ban uses the ingame identification code
# for kicking/banning.
def kickWorm(iID, reason = ""):
	if reason != "":
		print "kickworm " + str(iID) + " " + str(reason)
	else:
		print "kickworm " + str(iID)

def banWorm(iID, reason = ""):
	if reason != "":
		print "banworm " + str(iID) + " " + str(reason)
	else:
		print "banworm " + str(iID)

def muteWorm(iID):
	print "muteworm " + str(iID)

def setWormTeam_io(iID, team):
	print "setwormteam " + str(iID) + " " + str(team)

def authorizeWorm(iID):
	print "authorizeworm " + str(iID)

# Use this to get the list of all possible bots.
def getComputerWormList():
	print "getcomputerwormlist"
	resp = getResponseList("computerwormlistinfo")
	global bots
	bots = {}
	for r in resp:
		iID = int(r[:r.find(" ")])
		name = r[r.find(" ")+1:]
		bots[iID] = name

def getWormIP(iID):
	print "getwormip %i" % int(iID)
	return getResponse("wormip %i" % int(iID))

def getWormLocationInfo(iID):
	print "getwormlocationinfo %i" % int(iID)
	return getResponse("wormlocationinfo %i" % int(iID))

def getWormPing(iID):
	print "getwormping %i" % int(iID)
	return int(getResponse("wormping %i" % int(iID)))

# Use this to write to stdout (standard output)
def msg(string):
	print "msg " + str(string)

# Send a chat message
def chatMsg(string):
	print "chatmsg " + str(string)

# Send a private chat message
def privateMsg(iID, string):
	print "privatemsg %i %s" % ( int(iID), str(string) )


#Log Severity
LOG_CRITICAL = 0 # For things that you REALLY need to exit for.
LOG_ERROR = 1
LOG_WARN = 2
LOG_INFO = 3
# Categories for specific things, perhaps put in a new file?
# These are not in direct relation to the script.
LOG_ADMIN = 4

def messageLog(message,severity):
	# TODO: Allow setting what loglevels you want logged
	outline = time.strftime("%Y-%m-%d %H:%M:%S")
	# Don't clutter the strftime call
	outline += " -- "
	if severity == LOG_CRITICAL:
		outline += "CRITICAL"
	elif severity == LOG_ERROR:
		outline += "ERROR"
	elif severity == LOG_WARN:
		outline += "WARN"
	elif severity == LOG_INFO:
		outline += "INFO"
	elif severity == LOG_ADMIN: #Log to another file?
		outline += "ADMIN"

	outline += " -- "
	outline += str(message) #incase we get anything other than string
	try:
		f = open(cfg.LOG_FILE,"a")
		f.write((outline + "\n"))
		f.close()
	except IOError:
		msg("ERROR: Unable to open logfile.")

	#It's possible that we get a broken pipe here, but we can't exit clearly and also display it,
	# so let python send out the ugly warning.
	msg(outline)
