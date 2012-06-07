#!/usr/bin/python
# coding=utf-8
#
# The Doomsday Build Pilot
# (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not: http://www.opensource.org/

## @file pilot.py
##
## The build pilot runs on the autobuilder systems. On the host it manages
## tasks for the builder client systems. It listens on a TCP port for incoming
## queries and actions. On the clients the pilot is run periodically by cron,
## and any new tasks are carried out.
##
## The pilot's responsibility is distributed task management; the autobuild
## script carries out the actual tasks.

import sys
import os
import platform
import subprocess
import pickle
import struct
import time
import SocketServer

def homeDir():
    """Determines the path of the pilot home directory."""
    if 'Windows' in platform.platform():
        return os.path.join(os.getenv('USERPROFILE'), '.pilot')
    return os.path.join(os.getenv('HOME'), '.pilot')

# Get the configuration.
try:
    sys.path.append(homeDir())
    import pilotcfg
except ImportError:
    print """Configuration needed: pilotcfg.py must be created in ~/.pilot/

pilotcfg.py contains information such as (global variables):
- HOST: pilot server address (for clients)
- PORT: pilot server port
- ID: identifier of the build system
- DISTRIB_DIR: distrib directory path
- EVENTS_DIR: events directory path
- APT_DIR: apt repository path (for Linux systems)
- IGNORED_TASKS: list of tasks to quietly ignore (marked as complete)

The function 'postTaskHook(task)' can be defined for actions to be carried out 
after a successful execution of a task."""
    sys.exit(1)

APP_NAME = 'Doomsday Build Pilot'

def main():
    checkHome()
    checkMasterActions()    

    startNewPilotInstance()
    try:
        if isServer():
            listen()
        else:
            # Client mode. Check quietly for new tasks.
            checkForTasks()
    except Exception, x:
        # Unexpected problem!
        print APP_NAME + ':', x
    finally:
        endPilotInstance()

def msg(s):
    print >> sys.stderr, s


def isServer():
    return 'server' in sys.argv


def checkHome():
    if not os.path.exists(homeDir()):
        raise Exception(".pilot home directory does not exist.")


def branchFileName():
    return os.path.join(homeDir(), 'branch')


def currentBranch():
    if not os.path.exists(branchFileName()):
        return 'master' # default branch
    return file(branchFileName(), 'rt').read().strip()


def switchToBranch(branch):
    f = file(branchFileName(), 'wt')
    print >> f, branch
    f.close()


def checkMasterActions():
    """Special master actions."""
    if len(sys.argv) < 2: return
    
    if sys.argv[1] == 'new':
        assert pilotcfg.ID == 'master'
        
        # Create a new task: new sysid[,sysid]* taskname
        target = sys.argv[2]
        taskName = sys.argv[3]
        if target == 'ALL':
            newTask(taskName, allClients=True)
        else:
            for tgt in target.split(','):
                newTask(taskName, forClient=tgt)
        sys.exit(0)
        
    if sys.argv[1] == 'finish':
        assert pilotcfg.ID == 'master'
        
        # Check for completed tasks.
        handleCompletedTasks()
        sys.exit(0)    


def pidFileName():
    if isServer():
        return 'server.pid'
    else:
        return 'client.pid'


def isStale(fn):
    """Files are considered stale after some time has passed."""
    age = time.time() - os.stat(fn).st_ctime
    if age > 4*60*60:
        msg(fn + ' is stale, ignoring it.')
        return True
    return False


def startNewPilotInstance():
    """A new pilot instance can only be started if one is not already running
    on the system. If an existing instance is detected, this one will quit
    immediately."""
    # Check for an existing pid file.
    pid = os.path.join(homeDir(), pidFileName())
    if os.path.exists(pid):
        if not isStale(pid):
            # Cannot start right now -- will be retried later.
            sys.exit(0)
    print >> file(pid, 'w'), str(os.getpid())
    
    
def endPilotInstance():
    """Ends this pilot instance."""
    os.remove(os.path.join(homeDir(), pidFileName()))
    
    
def listTasks(clientId=None, includeCompleted=True, onlyCompleted=False,
              allClients=False):
    tasks = []

    for name in os.listdir(homeDir()):
        fn = os.path.join(homeDir(), name)
        if fn == '.' or fn == '..': continue

        # All tasks are specific to a client.
        if os.path.isdir(fn) and (name == clientId or allClients):
            for subname in os.listdir(fn):
                # Every task begins with the task prefix.
                if not subname.startswith('task_'): continue
                subfn = os.path.join(fn, subname)
                if not os.path.isdir(subfn):
                    tasks.append(subname[5:]) # Remove prefix.
    
    if not includeCompleted:
        tasks = filter(lambda n: not n.endswith('.done'), tasks)
        
    if onlyCompleted:
        tasks = filter(lambda n: isTaskComplete(n), tasks)

    tasks.sort()
    return tasks
            
            
def packs(s):
    """The length of the string is prefixed as a 32-bit integer in network
    byte order."""
    return struct.pack('!i', len(s)) + s

        
class ReqHandler(SocketServer.StreamRequestHandler):
    """Handler for requests from clients."""
    
    def handle(self):
        try:
            bytes = struct.unpack('!i', self.rfile.read(4))[0]
            self.request = pickle.loads(self.rfile.read(bytes))
            if type(self.request) != dict:
                raise Exception("Requests must be of type 'dict'")
            self.doRequest()
        except Exception, x:
            msg('Request failed: ' + str(x))
            response = { 'result': 'error', 'error': str(x) }
            self.respond(response)
            
    def respond(self, rsp):
        self.wfile.write(packs(pickle.dumps(rsp, 2)))
            
    def clientId(self):
        if 'id' in self.request:
            return self.request['id']
        else:
            return None

    def doRequest(self):
        if 'query' in self.request:
            self.doQuery()
        elif 'action' in self.request:
            self.doAction()
        else:
            raise Exception("Unknown request")
             
    def doQuery(self):
        qry = self.request['query']
        if qry == 'get_tasks':
            # Returns the tasks that a client should work on next.
            self.respond({ 'tasks': listTasks(self.clientId(),
                includeCompleted=False), 'result': 'ok' })
        else:
            raise Exception("Unknown query: " + qry)
              
    def doAction(self):
        act = self.request['action']
        if act == 'complete_task':
            completeTask(self.request['task'], self.clientId())
            self.respond({ 'result': 'ok', 'did_action': act })
        else:
            raise Exception("Unknown action: " + act)
    
        
def listen():
    print APP_NAME + ' starting in server mode (port %i).' % pilotcfg.PORT
    server = SocketServer.TCPServer(('0.0.0.0', pilotcfg.PORT), ReqHandler)
    server.serve_forever()


def query(q):
    """Sends a query to the server and returns the result."""
    
    import socket
    response = None
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((pilotcfg.HOST, pilotcfg.PORT))
        sock.send(packs(pickle.dumps(q, 2)))
        bytes = struct.unpack('!i', sock.recv(4))[0]
        response = pickle.loads(sock.recv(bytes))
    finally:
        sock.close()
    return response

    
def checkForTasks():
    for task in query({'id': pilotcfg.ID, 'query': 'get_tasks'})['tasks']:
        if not doTask(task): 
            # Ignore this task... (It will be done later.)
            continue 
        
        # Check for a post-task hook.
        if 'postTaskHook' in dir(pilotcfg):
            pilotcfg.postTaskHook(task)

        # No exception was thrown -- the task was successful.
        query({'action': 'complete_task', 
               'task': task, 
               'id': pilotcfg.ID})
            

def doTask(task):
    """Throws an exception if the task fails."""

    # Are we supposed to ignore this task?
    if 'IGNORED_TASKS' in dir(pilotcfg) and \
        task in pilotcfg.IGNORED_TASKS:
        return True

    if task == 'patch':
        msg("PATCH")
        switchToBranch('stable')
        return True
        
    elif task == 'branch_stable':
        msg("SWITCH TO STABLE BRANCH")
        switchToBranch('stable')
        return autobuild('pull')
        
    elif task == 'branch_master':
        msg("SWITCH TO MASTER BRANCH")
        switchToBranch('master')
        return autobuild('pull')

    elif task == 'tag_build':
        msg("TAG MASTER BRANCH")
        return autobuild('create')

    elif task == 'deb_changes':
        msg("UPDATE .DEB CHANGELOG")
        return autobuild('debchanges')

    elif task == 'build':
        msg("BUILD RELEASE")
        return autobuild('platform_release')

    elif task == 'sign':
        msg("SIGN PACKAGES")
        return autobuild('sign')

    elif task == 'publish':
        msg("PUBLISH")
        systemCommand('deng_copy_build_to_sourceforge.sh')

    elif task == 'apt_refresh':
        msg("APT REPOSITORY REFRESH")
        return autobuild('apt')
        
    elif task == 'update_feed':
        msg("UPDATE FEED")
        autobuild('feed')
        autobuild('xmlfeed')
        
    elif task == 'purge':
        msg("PURGE")
        return autobuild('purge')
        
    elif task == 'generate_readme':
        msg("GENERATE README")
        return autobuild('readme')
        
    elif task == 'generate_apidoc':
        msg("GENERATE API DOCUMENTATION")
        return autobuild('apidoc')
        
    elif task == 'mirror':
        msg("MIRROR")
        systemCommand('mirror-buildmaster-to-idisk.sh')

    return True
    

def handleCompletedTasks():
    """Check the completed tasks and see if we should start new tasks."""

    while True:
        tasks = listTasks(allClients=True, onlyCompleted=True)
        if len(tasks) == 0: break

        task = tasks[0][:-5] # Remove '.done'
        assert isTaskComplete(task)
        
        clearTask(task)
        
        print "Task '%s' has been completed (noticed at %s)" % (task, time.asctime())
        
        if task == 'patch':
            # Everyone must switch to the stable branch.
            newTask('branch_stable', allClients=True)
            
        elif task == 'branch_stable':
            # Commence with a build when everyone is ready.
            newTask('tag_build', forClient='master')
        
        elif task == 'tag_build':
            newTask('deb_changes', forClient='ubuntu')
            newTask('generate_readme', forClient='clikits')
        
        elif task == 'deb_changes':
            newTask('build', allClients=True)
        
        elif task == 'build':
            newTask('sign', forClient='master')

        elif task == 'sign':
            newTask('publish', forClient='master')
            newTask('apt_refresh', forClient='ubuntu')
            # After the build we can switch to the master again.
            newTask('branch_master', allClients=True)
            
        elif task == 'publish':
            newTask('update_feed', forClient='master')
            
        elif task == 'update_feed':
            newTask('mirror', forClient='master')
    
    
def autobuild(cmd):
    cmdLine = "python %s %s" % (os.path.join(pilotcfg.DISTRIB_DIR,
                                             'autobuild.py'), cmd)
    cmdLine += " --distrib %s" % pilotcfg.DISTRIB_DIR
    if 'EVENTS_DIR' in dir(pilotcfg):
        cmdLine += " --events %s" % pilotcfg.EVENTS_DIR
    if 'APT_DIR' in dir(pilotcfg):
        cmdLine += " --apt %s" % pilotcfg.APT_DIR

    cmdLine += " --branch %s" % currentBranch()

    systemCommand(cmdLine)
    return True
        
    
def systemCommand(cmd):
    result = subprocess.call(cmd, shell=True)
    if result != 0:
        raise Exception("Error from " + cmd)
    
    
def newTask(name, forClient=None, allClients=False):
    if allClients:
        for fn in os.listdir(homeDir()):
            if os.path.isdir(os.path.join(homeDir(), fn)):
                newTask(name, fn)
        return
                
    path = os.path.join(homeDir(), forClient)
    print "New task '%s' for client '%s'" % (name, forClient)
    
    print >> file(os.path.join(path, 'task_' + name), 'wt'), time.asctime()
        
        
def completeTask(name, byClient):
    path = os.path.join(homeDir(), byClient, 'task_' + name)
    if not os.path.exists(path):
        raise Exception("Cannot complete missing task '%s' (by client '%s')" % (name, byClient))

    print "Task '%s' completed by '%s' at" % (name, byClient), time.asctime()    
    os.rename(path, path + '.done')

        
def clearTask(name, direc=None):
    """Delete all task files with this name."""
    if not direc: direc = homeDir()
    for fn in os.listdir(direc):
        p = os.path.join(direc, fn)
        if os.path.isdir(p):
            clearTask(name, p)            
        if fn.startswith('task_') and (fn[5:] == name or \
            fn[5:] == name + '.done'):
            os.remove(p)


def isTaskComplete(name):
    # Remote the possible '.done' suffix.
    if name[-5:] == '.done': name = name[:-5]
    # Check that everyone has completed it.
    for task in listTasks(allClients=True):
        if task.startswith(name) and not task.endswith('.done'):
            # This one is not complete.
            return False
    return True
               

if __name__ == '__main__':
    main()
