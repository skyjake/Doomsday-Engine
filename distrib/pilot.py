#!/usr/bin/python
# coding=utf-8
#
# The Doomsday Build Pilot
# (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
## pilotcfg.py must be created in the .pilot home directory. It contains 
## information such as:
## - pilot server address (for clients) [HOST]
## - pilot server port [PORT]
## - identifier of the build system [ID]
## - distrib directory path [DISTRIB_DIR]
## - events directory path [EVENTS_DIR]
## - apt repository path (if present on system) [APT_DIR]

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
sys.path.append(homeDir())
import pilotcfg

APP_NAME = 'Doomsday Build Pilot'

def main():
    checkHome()
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


def isServer():
    return 'server' in sys.argv


def checkHome():
    if not os.path.exists(homeDir()):
        raise Exception(".pilot home directory does not exist.")


def pidFileName():
    if isServer():
        return 'server.pid'
    else:
        return 'client.pid'


def startNewPilotInstance():
    """A new pilot instance can only be started if one is not already running
    on the system. If an existing instance is detected, this one will quit
    immediately."""
    # Check for an existing pid file.
    pid = os.path.join(homeDir(), pidFileName())
    if os.path.exists(pid):
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

        # Tasks specific to a client.
        if os.path.isdir(fn) and (name == clientId or allClients):
            for subname in os.listdir(fn):
                if not subname.startswith('task_'): continue
                subfn = os.path.join(fn, subname)
                if not os.path.isdir(subfn):
                    tasks.append(subname[5:])

        # Common tasks.
        elif not os.path.isdir(fn) and name.startswith('task_'):
            tasks.append(name[5:])
    
    if not includeCompleted:
        tasks = filter(lambda n: not n.endswith('.done'), tasks)
        
    if onlyCompleted:
        tasks = filter(lambda n: n.endswith('.done'), tasks)

    tasks.sort()
    return tasks
            
            
def packs(s):
    return struct.pack('!i', len(s)) + s

        
class ReqHandler(SocketServer.StreamRequestHandler):
    def handle(self):
        try:
            bytes = struct.unpack('!i', self.rfile.read(4))[0]
            self.request = pickle.loads(self.rfile.read(bytes))
            if type(self.request) != dict:
                raise Exception("Requests must be of type 'dict'")
            self.doRequest()
        except Exception, x:
            print 'Request failed:', x
            response = { 'result': 'error', 'error': str(x) }
            self.respond(response)
            
    def respond(self, rsp):
        self.wfile.write(packs(pickle.dumps(rsp, 2)))
            
    def doRequest(self):
        if 'query' in self.request:
            if self.request['query'] == 'get_tasks':
                self.newTasksForClient()
            else:
                raise Exception("Unknown query: " + self.request['query'])
        elif 'action' in self.request:
            self.doAction()
        else:
            raise Exception("Unknown request")
             
    def clientId(self):
        if 'id' in self.request:
            return self.request['id']
        else:
            return None
              
    def newTasksForClient(self):
        """Returns the tasks that a client should work on next."""
        self.respond({ 'tasks': listTasks(self.clientId(), includeCompleted=False), 'result': 'ok' })

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
    if 'finish' in sys.argv:
        # Check for completed tasks.
        handleCompletedTasks()
    else:
        for task in query({'id': pilotcfg.ID, 'query': 'get_tasks'})['tasks']:
            # Try to do this task.
            doTask(task)
            # No exception was thrown -- the task was successful.
            query({'action': 'complete_task', 
                   'task': task, 
                   'id': pilotcfg.ID})
            

def doTask(task):
    """Throws an exception if the task fails."""

    if task == 'tag_build':
        autobuild('create')

    elif task == 'deb_changes':
        autobuild('debchanges')

    elif task == 'build':
        autobuild('platform_release')

    elif task == 'publish':
        systemCommand('deng_copy_build_to_sourceforge.sh')

    elif task == 'apt_refresh':
        autobuild('apt')

    elif task == 'update_feed':
        autobuild('feed')
        autobuild('xmlfeed')
    

def handleCompletedTasks():
    """Check the completed tasks and see if we should start new tasks."""

    while True:
        tasks = listTasks(allClients=True, onlyCompleted=True)
        if len(tasks) == 0: break

        task = tasks[0]
        if not isTaskComplete(task): continue
        
        clearTask(task)
        
        print "Task '%s' has been completed (noticed at %s)" % (task, time.asctime())
        
        if task == 'tag_build':
            # Process to generating the list of changes.
            newTask('deb_changes', allClients=True)
        
        elif task == 'deb_changes':
            newTask('build', allClients=True)            
        
        elif task == 'build':
            newTask('publish', forClient='master')
            newTask('apt_refresh', forClient='ubuntu')
            
        elif task == 'publish':
            newTask('update_feed', forClient='master')
    
    
def autobuild(cmd):
    if cmd == 'apt' and 'APT_DIR' not in pilotcfg.APT_DIR:
        # Ignore this...
        return
    
    cmdLine = "python %s %s" % (os.path.join(pilotcfg.DISTRIB_DIR,
                                             'autobuild.py'), cmd)
    cmdLine += " --distrib %s" % pilotcfg.DISTRIB_DIR
    if 'EVENTS_DIR' in dir(pilotcfg):
        cmdLine += " --events %s" % pilotcfg.EVENTS_DIR
    if 'APT_DIR' in dir(pilotcfg):
        cmdLine += " --apt %s" % pilotcfg.APT_DIR

    systemCommand(cmdLine)
        
    
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
                
    path = homeDir()
    if forClient:
        path = os.path.join(path, forClient)
        print "New task '%s' for client '%s'" % (name, forClient)
    else:
        print "New common task '%s'" % name
    
    print >> file(os.path.join(path, 'task_' + name), 'wt'), time.asctime()
        
        
def completeTask(name, byClient, quiet=False):
    if byClient:
        path = os.path.join(homeDir(), byClient, 'task_' + name)
        if not os.path.exists(path):
            # Maybe it's a common task?
            completeTask(name, quiet=True)
            return
    else:
        path = os.path.join(homeDir(), 'task_' + name)

    if not os.path.exists(path):
        raise Exception("Cannot complete missing task '%s'" % name)
        
    if not quiet:
        if byClient:
            print "Task '%s' completed by '%s' at" % (name, byClient), time.asctime()
        else:
            print "Task '%s' completed at", time.asctime()
        
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
    for task in listTasks(allClients=True):
        if task.startswith(name) and not task.endswith('.done'):
            # This one is not complete.
            return False
    return True
               

if __name__ == '__main__':
    main()
