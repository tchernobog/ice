// **********************************************************************
//
// Copyright (c) 2003-2006 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <IceUtil/DisableWarnings.h>
#include <IceUtil/OutputUtil.h>
#include <IceUtil/Options.h>
#include <Ice/Ice.h>
#include <IceXML/Parser.h>
#include <IceGrid/Parser.h>
#include <IceGrid/Util.h>
#include <IceGrid/DescriptorParser.h>
#include <IceGrid/DescriptorHelper.h>

#ifdef HAVE_READLINE
#   include <readline/readline.h>
#   include <readline/history.h>
#endif

#include <iterator>
#include <iomanip>

extern FILE* yyin;
extern int yydebug;

using namespace std;
using namespace IceUtil;
using namespace Ice;
using namespace IceGrid;

namespace IceGrid
{

Parser* parser;

}

ParserPtr
Parser::createParser(const CommunicatorPtr& communicator, const AdminSessionPrx& session, const AdminPrx& admin,
		     bool interactive)
{
    return new Parser(communicator, session, admin, interactive);
}

void
Parser::usage()
{
    cout <<
        "help                        Print this message.\n"
        "exit, quit                  Exit this program.\n"
	"\n"
	"application add [-n | --no-patch] DESC [TARGET ... ] [NAME=VALUE ... ]\n"
	"                            Add application described in DESC. If specified\n"
        "                            the optional targets TARGET will be deployed.\n"
	"application remove NAME     Remove application NAME.\n"
	"application describe NAME   Describe application NAME.\n"
	"application diff DESC [TARGET ... ] [NAME=VALUE ... ]\n"
        "                            Print the differences betwen the application\n"
        "                            described in DESC and the current deployment.\n"
	"application update DESC [TARGET ... ] [NAME=VALUE ... ]\n"
	"                            Update the application described in DESC.\n"
	"application patch [-f | --force] NAME\n"
	"                            Patch the given application data. If -f or --force is\n"
	"                            specified, the servers depending on the data to patch\n"
	"                            will be stopped if necessary.\n"
	"application list            List all deployed applications.\n"
        "\n"
	"server template instantiate APPLICATION NODE TEMPLATE [NAME=VALUE ...]\n"
	"                            Instantiate a server template.\n"
	"server template describe APPLICATION TEMPLATE\n"
        "                            Describe application server template TEMPLATE.\n"
	"\n"
	"service template describe APPLICATION TEMPLATE\n"
        "                            Describe application service template TEMPLATE.\n"
	"\n"
	"node list                   List all registered nodes.\n"
        "node describe NAME          Show information about node NAME.\n"
	"node ping NAME              Ping node NAME.\n"
	"node load NAME              Print the load of the node NAME.\n"
	"node dump stderr NAME       Dump node NAME stderr.\n"
	"node dump stdout NAME       Dump node NAME stdout.\n"
	"node shutdown NAME          Shutdown node NAME.\n"
	"\n"
	"registry list               List all registered registrys.\n"
        "registry describe NAME      Show information about registry NAME.\n"
	"registry ping NAME          Ping registry NAME.\n"
	"registry dump stderr NAME   Dump registry NAME stderr.\n"
	"registry dump stdout NAME   Dump registry NAME stdout.\n"
	"registry shutdown NAME      Shutdown registry NAME.\n"
	"\n"
        "server list                 List all registered servers.\n"
        "server remove ID            Remove server ID.\n"
        "server describe ID          Describe server ID.\n"
	"server state ID             Get the state of server ID.\n"
	"server pid ID               Get the process id of server ID.\n"
	"server start ID             Start server ID.\n"
	"server stop ID              Stop server ID.\n"
	"server patch ID             Patch server ID.\n"
        "server signal ID SIGNAL     Send SIGNAL (e.g. SIGTERM or 15) to server ID.\n"
        "server stdout ID MESSAGE    Write MESSAGE on server ID's stdout.\n"
	"server stderr ID MESSAGE    Write MESSAGE on server ID's stderr.\n"
	"server dump stderr ID       Dump server ID stderr.\n"
	"server dump stdout ID       Dump server ID stdout.\n"
	"server enable ID            Enable server ID.\n"
	"server disable ID           Disable server ID (a disabled server can't be\n"
        "                            started on demand or administratively).\n"
	"\n"
        "adapter list                List all registered adapters.\n"
	"adapter endpoints ID        Show the endpoints of adapter or replica group ID.\n"
	"adapter remove ID           Remove adapter or replica group ID.\n"
	"\n"
	"object add PROXY [TYPE]     Add an object to the object registry,\n"
	"                            optionally specifying its type.\n"
	"object remove IDENTITY      Remove an object from the object registry.\n"
	"object find TYPE            Find all objects with the type TYPE.\n"
	"object describe EXPR        Describe all registered objects whose stringified\n"
        "                            identities match the expression EXPR. A trailing\n"
	"                            wildcard is supported in EXPR, for example\n"
	"                            \"object describe Ice*\".\n"
	"object list EXPR            List all registered objects whose stringified\n"
        "                            identities match the expression EXPR. A trailing\n"
	"                            wildcard is supported in EXPR, for example\n"
	"                            \"object list Ice*\".\n"
	"\n"
        "shutdown                    Shut the IceGrid registry down.\n"
	;
}

void
Parser::interrupt()
{
    Lock sync(*this);
    _interrupted = true;
    notifyAll();
}

bool
Parser::interrupted() const
{
    Lock sync(*this);
    return _interrupted;
}

void
Parser::resetInterrupt()
{
    Lock sync(*this);
    _interrupted = false;
}

void
Parser::checkInterrupted()
{
    if(!_interactive)
    {
	Lock sync(*this);
	if(_interrupted)
	{
	    throw "interrupted with Ctrl-C";
	}
    }
}

void
Parser::addApplication(const list<string>& origArgs)
{
    list<string> copyArgs = origArgs;
    copyArgs.push_front("icegridadmin");
    
    IceUtil::Options opts;
    opts.addOpt("n", "no-patch");
    vector<string> args;
    try
    {
	for(list<string>::const_iterator p = copyArgs.begin(); p != copyArgs.end(); ++p)
	{
	    args.push_back(*p);
	}
	args = opts.parse(args);
    }
    catch(const IceUtil::BadOptException& e)
    {
	error(e.reason);
	return;
    }

    if(args.size() < 1)
    {
	invalidCommand("`application add' requires at least one argument");
	return;
    }

    try
    {
	StringSeq targets;
	map<string, string> vars;

	vector<string>::const_iterator p = args.begin();
	string desc = *p++;

	for(; p != args.end(); ++p)
	{
	    string::size_type pos = p->find('=');
	    if(pos != string::npos)
	    {
		vars[p->substr(0, pos)] = p->substr(pos + 1);
	    }
	    else
	    {
		targets.push_back(*p);
	    }
	}

	//
	// Add the application.
	//
	ApplicationDescriptor app = DescriptorParser::parseDescriptor(desc, targets, vars, _communicator, _admin);
	_admin->addApplication(app);
	
	if(!opts.isSet("no-patch"))
	{
	    //
	    // Patch the application.
	    //
	    try
	    {
		_admin->patchApplication(app.name, true);
	    }
	    catch(const PatchException& ex)
	    {
		patchFailed(ex.reasons);
	    }
	}
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::removeApplication(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`application remove' requires exactly one argument");
	return;
    }

    try
    {
	list<string>::const_iterator p = args.begin();

	string name = *p++;

	_admin->removeApplication(name);
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::describeApplication(const list<string>& args)
{
    if(args.size() < 1)
    {
	invalidCommand("`application describe' requires at least one argument");
	return;
    }

    try
    {
	list<string>::const_iterator p = args.begin();

	string name = *p++;

	Output out(cout);
	ApplicationInfo info = _admin->getApplicationInfo(name);
	ApplicationHelper helper(_communicator, info.descriptor);
	helper.print(out, info);
	out << nl;
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::diffApplication(const list<string>& args)
{
    if(args.size() < 1)
    {
	invalidCommand("`application diff' requires at least one argument");
	return;
    }

    try
    {
	StringSeq targets;
	map<string, string> vars;

	list<string>::const_iterator p = args.begin();
	string desc = *p++;

	for(; p != args.end(); ++p)
	{
	    string::size_type pos = p->find('=');
	    if(pos != string::npos)
	    {
		vars[p->substr(0, pos)] = p->substr(pos + 1);
	    }
	    else
	    {
		targets.push_back(*p);
	    }
	}

	ApplicationDescriptor newApp = DescriptorParser::parseDescriptor(desc, targets, vars, _communicator, _admin);
	ApplicationInfo origApp = _admin->getApplicationInfo(newApp.name);

	ApplicationHelper newAppHelper(_communicator, newApp);
	ApplicationHelper oldAppHelper(_communicator, origApp.descriptor);
	
	Output out(cout);
	newAppHelper.printDiff(out, oldAppHelper);
	out << nl;  
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::updateApplication(const list<string>& args)
{
    if(args.size() < 1)
    {
	invalidCommand("`application diff' requires at least one argument");
	return;
    }

    try
    {
	StringSeq targets;
	map<string, string> vars;

	list<string>::const_iterator p = args.begin();
	string desc = *p++;

	for(; p != args.end(); ++p)
	{
	    string::size_type pos = p->find('=');
	    if(pos != string::npos)
	    {
		vars[p->substr(0, pos)] = p->substr(pos + 1);
	    }
	    else
	    {
		targets.push_back(*p);
	    }
	}

	_admin->syncApplication(DescriptorParser::parseDescriptor(desc, targets, vars, _communicator, _admin));
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::patchApplication(const list<string>& origArgs)
{
    list<string> copyArgs = origArgs;
    copyArgs.push_front("icegridadmin");
    
    IceUtil::Options opts;
    opts.addOpt("f", "force");
    vector<string> args;
    try
    {
	for(list<string>::const_iterator p = copyArgs.begin(); p != copyArgs.end(); ++p)
	{
	    args.push_back(*p);
	}
	args = opts.parse(args);
    }
    catch(const IceUtil::BadOptException& e)
    {
	error(e.reason);
	return;
    }

    if(args.size() != 1)
    {
	invalidCommand("`application patch' requires exactly one argument");
	return;
    }

    try
    {
	vector<string>::const_iterator p = args.begin();
	string name = *p++;
	_admin->patchApplication(name, opts.isSet("force"));
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::listAllApplications()
{
    try
    {
	Ice::StringSeq names = _admin->getAllApplicationNames();
	copy(names.begin(), names.end(), ostream_iterator<string>(cout,"\n"));
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::describeServerTemplate(const list<string>& args)
{
    if(args.size() != 2)
    {
	invalidCommand("`server template describe' requires exactly two arguments");
	return;
    }

    try
    {
	list<string>::const_iterator p = args.begin();

	string name = *p++;
	string templ = *p++;

	ApplicationInfo application = _admin->getApplicationInfo(name);
	
	Output out(cout);
	TemplateDescriptorDict::const_iterator q = application.descriptor.serverTemplates.find(templ);
	if(q != application.descriptor.serverTemplates.end())
	{
	    out << "server template `" << templ << "'";
	    out << sb;

	    out << nl << "parameters = `" << toString(q->second.parameters) << "'";
	    out << nl;

	    ServerDescriptorPtr server = ServerDescriptorPtr::dynamicCast(q->second.descriptor);
	    IceBoxDescriptorPtr iceBox = IceBoxDescriptorPtr::dynamicCast(server);
	    if(iceBox)
	    {
		IceBoxHelper(_communicator, iceBox).print(out);
	    }
	    else
	    {
		ServerHelper(_communicator, server).print(out);
	    }
	    out << eb;
	    out << nl;
	}
	else
	{
	    error("no server template with id `" + templ + "'");
	}
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::instantiateServerTemplate(const list<string>& args)
{
    if(args.size() < 3)
    {
	invalidCommand("`server template instantiate' requires at least three arguments");
	return;
    }

    try
    {
	map<string, string> vars;

	list<string>::const_iterator p = args.begin();
	string application = *p++;
	string node = *p++;
	string templ = *p++;
	for(; p != args.end(); ++p)
	{
	    string::size_type pos = p->find('=');
	    if(pos != string::npos)
	    {
		vars[p->substr(0, pos)] = p->substr(pos + 1);
	    }
	}

	ServerInstanceDescriptor desc;
	desc._cpp_template = templ;
	desc.parameterValues = vars;
	_admin->instantiateServer(application, node, desc);
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::describeServiceTemplate(const list<string>& args)
{
    if(args.size() != 2)
    {
	invalidCommand("`service template describe' requires exactly two arguments");
	return;
    }

    try
    {
	list<string>::const_iterator p = args.begin();

	string name = *p++;
	string templ = *p++;

	ApplicationInfo application = _admin->getApplicationInfo(name);
	
	Output out(cout);
	TemplateDescriptorDict::const_iterator q = application.descriptor.serviceTemplates.find(templ);
	if(q != application.descriptor.serviceTemplates.end())
	{
	    out << "service template `" << templ << "'";
	    out << sb;

	    out << nl << "parameters = `" << toString(q->second.parameters) << "'";
	    out << nl;

	    ServiceDescriptorPtr desc = ServiceDescriptorPtr::dynamicCast(q->second.descriptor);
	    ServiceHelper(_communicator, desc).print(out);
	    out << eb;
	    out << nl;
	}
	else
	{
	    invalidCommand("no service template with id `" + templ + "'");
	}
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::describeNode(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`node describe' requires exactly one argument");
	return;
    }

    try
    {
	NodeInfo info = _admin->getNodeInfo(args.front());
	Output out(cout);
	out << "node `" << args.front() << "'";
	out << sb;
	out << nl << "operating system = `" << info.os << "'";
	out << nl << "host name = `" << info.hostname << "'";
	out << nl << "release = `" << info.release << "'";
	out << nl << "version = `" << info.version << "'";
	out << nl << "machine type = `" << info.machine << "'";
	out << nl << "number of processors = `" << info.nProcessors << "'";
	out << eb;
	out << nl;
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::pingNode(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`node ping' requires exactly one argument");
	return;
    }

    try
    {
	if(_admin->pingNode(args.front()))
	{
	    cout << "node is up" << endl;
	}
	else
	{
	    cout << "node is down" << endl;
	}
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::printLoadNode(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`node load' requires exactly one argument");
	return;
    }

    try
    {
	LoadInfo load = _admin->getNodeLoad(args.front());
	cout << "load average (1/5/15): " << load.avg1 << " / " << load.avg5 << " / " << load.avg15 << endl;
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::shutdownNode(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`node shutdown' requires exactly one argument");
	return;
    }

    try
    {
	_admin->shutdownNode(args.front());
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::listAllNodes()
{
    try
    {
	Ice::StringSeq names = _admin->getAllNodeNames();
	copy(names.begin(), names.end(), ostream_iterator<string>(cout,"\n"));
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::describeRegistry(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`registry describe' requires exactly one argument");
	return;
    }

    try
    {
	RegistryInfo info = _admin->getRegistryInfo(args.front());
	Output out(cout);
	out << "registry `" << args.front() << "'";
	out << sb;
	out << nl << "host name = `" << info.hostname << "'";
	out << nl << "endpoints = `" << info.endpoints << "'";
	out << eb;
	out << nl;
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::pingRegistry(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`registry ping' requires exactly one argument");
	return;
    }

    try
    {
	if(_admin->pingRegistry(args.front()))
	{
	    cout << "registry is up" << endl;
	}
	else
	{
	    cout << "registry is down" << endl;
	}
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::shutdownRegistry(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`registry shutdown' requires exactly one argument");
	return;
    }

    try
    {
	_admin->shutdownRegistry(args.front());
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::listAllRegistries()
{
    try
    {
	Ice::StringSeq names = _admin->getAllRegistryNames();
	copy(names.begin(), names.end(), ostream_iterator<string>(cout,"\n"));
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::removeServer(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`server start' requires exactly one argument");
	return;
    }

    try
    {
	ServerInfo info = _admin->getServerInfo(args.front());
	NodeUpdateDescriptor nodeUpdate;
	nodeUpdate.name = info.node;
	nodeUpdate.removeServers.push_back(args.front());
	ApplicationUpdateDescriptor update;
	update.name = info.application;
	update.nodes.push_back(nodeUpdate);
	_admin->updateApplication(update);
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }    
}

void
Parser::startServer(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`server start' requires exactly one argument");
	return;
    }

    try
    {
	_admin->startServer(args.front());
    }
    catch(const ServerStartException& ex)
    {
	error("the server didn't start successfully:\n" + ex.reason);
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::stopServer(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`server stop' requires exactly one argument");
	return;
    }

    try
    {
	_admin->stopServer(args.front());
    }
    catch(const ServerStopException& ex)
    {
	error("the server didn't stop successfully:\n" + ex.reason);
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::patchServer(const list<string>& origArgs)
{
    list<string> copyArgs = origArgs;
    copyArgs.push_front("icegridadmin");

    IceUtil::Options opts;
    opts.addOpt("f", "force");
    vector<string> args;
    try
    {
	for(list<string>::const_iterator p = copyArgs.begin(); p != copyArgs.end(); ++p)
	{
	    args.push_back(*p);
	}
	args = opts.parse(args);
    }
    catch(const IceUtil::BadOptException& e)
    {
	error(e.reason);
	return;
    }

    if(args.size() != 1)
    {
	invalidCommand("`server patch' requires exactly one argument");
	return;
    }

    try
    {
	_admin->patchServer(args.front(), opts.isSet("force"));
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::signalServer(const list<string>& args)
{
    if(args.size() != 2)
    {
	invalidCommand("`server signal' requires exactly two arguments");
	return;
    }

    try
    {
	list<string>::const_iterator p = args.begin();
	string server = *p++;
	_admin->sendSignal(server, *p);
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}


void
Parser::writeMessage(const list<string>& args, int fd)
{
    if(args.size() != 2)
    {
	invalidCommand("`server stdout or server stderr' requires exactly two arguments");
	return;
    }

    try
    {
	list<string>::const_iterator p = args.begin();
	string server = *p++;
	_admin->writeMessage(server, *p,  fd);
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::describeServer(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`server describe' requires exactly one argument");
	return;
    }
    
    try
    {
	ServerInfo info = _admin->getServerInfo(args.front());
	Output out(cout);
	IceBoxDescriptorPtr iceBox = IceBoxDescriptorPtr::dynamicCast(info.descriptor);
	if(iceBox)
	{
	    IceBoxHelper(_communicator, iceBox).print(out, info);
	}
	else
	{
	    ServerHelper(_communicator, info.descriptor).print(out, info);
	}
	out << nl;
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::stateServer(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`server state' requires exactly one argument");
	return;
    }

    try
    {
	ServerState state = _admin->getServerState(args.front());
	string enabled = _admin->isServerEnabled(args.front()) ? "enabled" : "disabled";
	switch(state)
	{
	case Inactive:
	{
	    cout << "inactive (" << enabled << ")" << endl;
	    break;
	}
	case Activating:
	{
	    cout << "activating (" << enabled << ")" << endl;
	    break;
	}
	case Active:
	{
	    int pid = _admin->getServerPid(args.front());
	    cout << "active (pid = " << pid << ", " << enabled << ")" << endl;
	    break;
	}
	case ActivationTimedOut:
	{
	    int pid = _admin->getServerPid(args.front());
	    cout << "activation timed out (pid = " << pid << ", " << enabled << ")" << endl;
	    break;
	}
	case Deactivating:
	{
	    cout << "deactivating (" << enabled << ")" << endl;
	    break;
	}
	case Destroying:
	{
 	    cout << "destroying (" << enabled << ")" << endl;
	    break;
	}
	case Destroyed:
	{
	    cout << "destroyed (" << enabled << ")" << endl;
	    break;
	}
	default:
	    assert(false);
	}
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::pidServer(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`server pid' requires exactly one argument");
	return;
    }

    try
    {
	int pid = _admin->getServerPid(args.front());
	if(pid > 0)
	{
	    cout << pid << endl;
	}
	else
	{
	    error("server is not running"); 
	}
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::enableServer(const list<string>& args, bool enable)
{
    if(args.size() != 1)
    {
	if(enable)
	{
	    invalidCommand("`server enable' requires exactly one argument");
	}
	else
	{
	    invalidCommand("`server disable' requires exactly one argument");
	}
	return;
    }

    try
    {
	_admin->enableServer(args.front(), enable);
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::listAllServers()
{
    try
    {
	Ice::StringSeq ids = _admin->getAllServerIds();
	copy(ids.begin(), ids.end(), ostream_iterator<string>(cout,"\n"));
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::endpointsAdapter(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`adapter endpoints' requires exactly one argument");
	return;
    }

    try
    {
	string adapterId = args.front();
	AdapterInfoSeq adpts = _admin->getAdapterInfo(adapterId);
	if(adpts.size() == 1 && adpts.begin()->id == adapterId)
	{
	    string endpoints = _communicator->proxyToString(adpts.begin()->proxy);
	    cout << (endpoints.empty() ? string("<inactive>") : endpoints) << endl;
	}
	else
	{
	    for(AdapterInfoSeq::const_iterator p = adpts.begin(); p != adpts.end(); ++p)
	    {
		cout << (p->id.empty() ? string("<empty>") : p->id) << ": ";
		string endpoints = _communicator->proxyToString(p->proxy);
		cout << (endpoints.empty() ? string("<inactive>") : endpoints) << endl;
	    }
	}
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::removeAdapter(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`adapter remove' requires exactly one argument");
	return;
    }

    try
    {
	_admin->removeAdapter(*args.begin());
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::listAllAdapters()
{
    try
    {
	Ice::StringSeq ids = _admin->getAllAdapterIds();
	copy(ids.begin(), ids.end(), ostream_iterator<string>(cout,"\n"));
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::addObject(const list<string>& args)
{
    if(args.size() < 1)
    {
	invalidCommand("`object add' requires at least one argument");
	return;
    }

    try
    {
	list<string>::const_iterator p = args.begin();

	string proxy = *p++;

	if(p != args.end())
	{
	    string type = *p++;
	    _admin->addObjectWithType(_communicator->stringToProxy(proxy), type);
	}
	else
	{
	    _admin->addObject(_communicator->stringToProxy(proxy));
	}
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::removeObject(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`object remove' requires exactly one argument");
	return;
    }

    try
    {
	_admin->removeObject(_communicator->stringToIdentity((*(args.begin()))));
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::findObject(const list<string>& args)
{
    if(args.size() != 1)
    {
	invalidCommand("`object find' requires exactly one argument");
	return;
    }

    try
    {
	ObjectInfoSeq objects = _admin->getObjectInfosByType(*(args.begin()));
	for(ObjectInfoSeq::const_iterator p = objects.begin(); p != objects.end(); ++p)
	{
	    cout << _communicator->proxyToString(p->proxy) << endl;
	}	
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::describeObject(const list<string>& args)
{
    try
    {
	ObjectInfoSeq objects;
	if(args.size() == 1)
	{
	    string arg = *(args.begin());
	    if(arg.find('*') == string::npos)
	    {
		ObjectInfo info = _admin->getObjectInfo(_communicator->stringToIdentity(arg));
		cout << "proxy = `" << _communicator->proxyToString(info.proxy) << "'" << endl;
		cout << "type = `" << info.type << "'" << endl;
		return;
	    }
	    else
	    {
		objects = _admin->getAllObjectInfos(arg);
	    }
	}
	else
	{
	    objects = _admin->getAllObjectInfos("");
	}
	
	for(ObjectInfoSeq::const_iterator p = objects.begin(); p != objects.end(); ++p)
	{
	    cout << "proxy = `" << _communicator->proxyToString(p->proxy) << "' type = `" << p->type << "'" << endl;
	}
	
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::listObject(const list<string>& args)
{
    try
    {
	ObjectInfoSeq objects;
	if(args.size() == 1)
	{
	    objects = _admin->getAllObjectInfos(*(args.begin()));
	}
	else
	{
	    objects = _admin->getAllObjectInfos("");
	}
	
	for(ObjectInfoSeq::const_iterator p = objects.begin(); p != objects.end(); ++p)
	{
	    cout << _communicator->identityToString(p->proxy->ice_getIdentity()) << endl;
	}	
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::shutdown()
{
    try
    {
	_admin->shutdown();
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::dumpFile(const string& reader, const string& filename, const list<string>& origArgs)
{
    list<string> copyArgs = origArgs;
    copyArgs.push_front("icegridadmin");
    
    IceUtil::Options opts;
    opts.addOpt("f", "follow");
    opts.addOpt("h", "head", IceUtil::Options::NeedArg);
    opts.addOpt("t", "tail", IceUtil::Options::NeedArg);

    vector<string> args;
    try
    {
	for(list<string>::const_iterator p = copyArgs.begin(); p != copyArgs.end(); ++p)
	{
	    args.push_back(*p);
	}
	args = opts.parse(args);
    }
    catch(const IceUtil::BadOptException& e)
    {
	error(e.reason);
	return;
    }

    if(args.size() != 1)
    {
	invalidCommand("`" + reader + " dump " + filename + "' requires one argument");
	return;
    }

    try
    {
	string id = *(args.begin());
	FileIteratorPrx it;
	if(reader == "node")
	{
	    if(filename == "stderr")
	    {
		it = _session->openNodeStdErr(id);
	    }
	    else if(filename == "stdout")
	    {
		it = _session->openNodeStdOut(id);
	    }
	}
	else if(reader == "registry")
	{
	    if(filename == "stderr")
	    {
		it = _session->openRegistryStdErr(id);
	    }
	    else if(filename == "stdout")
	    {
		it = _session->openRegistryStdOut(id);
	    }
	}
	else if(reader == "server")
	{
	    if(filename == "stderr")
	    {
		it = _session->openServerStdErr(id);
	    }
	    else if(filename == "stdout")
	    {
		it = _session->openServerStdOut(id);
	    }
	}

	cout << reader << " `" << id << "' " << filename << ":" << flush;
	Ice::StringSeq lines;

	bool head = opts.isSet("head");
	bool tail = opts.isSet("tail");
	if(head && tail)
	{
	    invalidCommand("can't specify both -h | --head and -t | --tail options");
	    return;
	}
	int lineCount = 20;
	if(head || tail)
	{
	    istringstream is(head ? opts.optArg("head") : opts.optArg("tail"));
	    is >> lineCount;
	    if(lineCount <= 0)
	    {
		invalidCommand("invalid argument for -h | --head or -t | --tail option");
		return;
	    }
	}

	bool follow = opts.isSet("follow");
	resetInterrupt();
	if(head)
	{
	    if(follow)
	    {
		invalidCommand("can't use -f | --follow option with -h | --head option");
		return;
	    }

	    int i = 0;
	    while(!interrupted())
	    {
		lines = it->read(20);

		Ice::StringSeq::const_iterator p = lines.begin();
		while(i < lineCount && p != lines.end())
		{
		    cout << endl << *p++ << flush;
		    ++i;
		}

		if(i == lineCount || lines.size() < 20)
		{
		    break;
		}
	    }
	}
	else if(tail)
	{
	    deque<string> lastLines;
	    while(!interrupted())
	    {
		lines = it->read(20);

		copy(lines.begin(), lines.end(), back_inserter(lastLines));
		int remove = lastLines.size() - lineCount;
		if(remove > 0)
		{
		    lastLines.erase(lastLines.begin(), lastLines.begin() + remove);
		    assert(lastLines.size() == static_cast<unsigned int>(lineCount));
		}

		if(lines.size() < 20)
		{
		    break;
		}
	    }
	    
	    for(deque<string>::const_iterator p = lastLines.begin(); p != lastLines.end(); ++p)
	    {
		cout << endl << *p << flush;
	    }
	}
	else
	{
	    while(!interrupted())
	    {
		lines = it->read(20);
		for(Ice::StringSeq::const_iterator p = lines.begin(); p != lines.end(); ++p)
		{
		    cout << endl << *p << flush;
		}

		if(lines.size() < 20)
		{
		    break;
		}
	    }
	}

	if(follow)
	{
	    while(!interrupted())
	    {
		lines = it->read(20);
		for(Ice::StringSeq::const_iterator p = lines.begin(); p != lines.end(); ++p)
		{
		    cout << *p;
		    if((p + 1) != lines.end())
		    {
			cout << endl;
		    }
		    else
		    {
			cout << flush;
		    }
		}

		{
		    Lock sync(*this);
		    if(_interrupted)
		    {
			break;
		    }
		    timedWait(IceUtil::Time::seconds(5));
		}
	    }
	}

	if(lines.empty() || !lines.back().empty())
	{
	    cout << endl;
	}

	it->destroy();
    }
    catch(const Ice::Exception& ex)
    {
	exception(ex);
    }
}

void
Parser::showBanner()
{
    cout << "Ice " << ICE_STRING_VERSION << "  Copyright 2003-2006 ZeroC, Inc." << endl;
}

void
Parser::showCopying()
{
    cout << "This command is not implemented." << endl;
}

void
Parser::showWarranty()
{
    cout << "This command is not implemented." << endl;
}

void
Parser::getInput(char* buf, int& result, int maxSize)
{
    if(!_commands.empty())
    {
	if(_commands == ";")
	{
	    result = 0;
	}
	else
	{
#if defined(_MSC_VER) && !defined(_STLP_MSVC)
	    // COMPILERBUG: Stupid Visual C++ defines min and max as macros
	    result = _MIN(maxSize, static_cast<int>(_commands.length()));
#else
	    result = min(maxSize, static_cast<int>(_commands.length()));
#endif
	    strncpy(buf, _commands.c_str(), result);
	    _commands.erase(0, result);
	    if(_commands.empty())
	    {
		_commands = ";";
	    }
	}
    }
    else if(isatty(fileno(yyin)))
    {
#ifdef HAVE_READLINE

        const char* prompt = parser->getPrompt();
	char* line = readline(const_cast<char*>(prompt));
	if(!line)
	{
	    result = 0;
	}
	else
	{
	    if(*line)
	    {
		add_history(line);
	    }

	    result = strlen(line) + 1;
	    if(result > maxSize)
	    {
		free(line);
		error("input line too long");
		result = 0;
	    }
	    else
	    {
		strcpy(buf, line);
		strcat(buf, "\n");
		free(line);
	    }
	}

#else

	cout << parser->getPrompt() << flush;

	string line;
	while(true)
	{
	    char c = static_cast<char>(getc(yyin));
	    if(c == EOF)
	    {
		if(line.size())
		{
		    line += '\n';
		}
		break;
	    }
	    line += c;

	    if(c == '\n')
	    {
		break;
	    }
	}
	
	result = (int) line.length();
	if(result > maxSize)
	{
	    error("input line too long");
	    buf[0] = EOF;
	    result = 1;
	}
	else
	{
	    strcpy(buf, line.c_str());
	}

#endif
    }
    else
    {
	if(((result = (int) fread(buf, 1, maxSize, yyin)) == 0) && ferror(yyin))
	{
	    error("input in flex scanner failed");
	    buf[0] = EOF;
	    result = 1;
	}
    }
}

void
Parser::nextLine()
{
    _currentLine++;
}

void
Parser::continueLine()
{
    _continue = true;
}

const char*
Parser::getPrompt()
{
    assert(_commands.empty() && isatty(fileno(yyin)));

    if(_continue)
    {
	_continue = false;
	return "(cont) ";
    }
    else
    {
	return ">>> ";
    }
}

void
Parser::scanPosition(const char* s)
{
    string line(s);
    string::size_type idx;

    idx = line.find("line");
    if(idx != string::npos)
    {
	line.erase(0, idx + 4);
    }

    idx = line.find_first_not_of(" \t\r#");
    if(idx != string::npos)
    {
	line.erase(0, idx);
    }

    _currentLine = atoi(line.c_str()) - 1;

    idx = line.find_first_of(" \t\r");
    if(idx != string::npos)
    {
	line.erase(0, idx);
    }

    idx = line.find_first_not_of(" \t\r\"");
    if(idx != string::npos)
    {
	line.erase(0, idx);

	idx = line.find_first_of(" \t\r\"");
	if(idx != string::npos)
	{
	    _currentFile = line.substr(0, idx);
	    line.erase(0, idx + 1);
	}
	else
	{
	    _currentFile = line;
	}
    }
}

void
Parser::invalidCommand(const char* s)
{
    error(string(s) + "\n(`help' for more info)");
}

void
Parser::invalidCommand(const string& s)
{
    invalidCommand(s.c_str());
}

void
Parser::patchFailed(const Ice::StringSeq& reasons)
{
    ostringstream os;
    IceUtil::Output out(os);
    out.setIndent(2);
    out << "the patch failed on some nodes:\n";
    for(Ice::StringSeq::const_iterator p = reasons.begin(); p != reasons.end(); ++p)
    {
	string reason = *p;
	string::size_type beg = 0;
	string::size_type end = reason.find_first_of("\n");
	if(end == string::npos)
	{
	    end = reason.size();
	}
	out << "- " << reason.substr(beg, end - beg);
	out.inc();
	while(end < reason.size())
	{
	    beg = end + 1;
	    end = reason.find_first_of("\n", beg);
	    if(end == string::npos)
	    {
		end = reason.size();
	    }
	    out.newline();
	    out << reason.substr(beg, end - beg);
	}
	out.dec();
	if(p + 1 != reasons.end())
	{
	    out.newline();
	}
    }
    warning(os.str());
}

void
Parser::error(const char* s)
{
    if(_commands.empty() && !isatty(fileno(yyin)))
    {
	cerr << _currentFile << ':' << _currentLine << ": " << s << endl;
    }
    else
    {
	cerr << "error: " << s << endl;
    }
    _errors++;
}

void
Parser::error(const string& s)
{
    error(s.c_str());
}

void
Parser::warning(const char* s)
{
    if(_commands.empty() && !isatty(fileno(yyin)))
    {
	cerr << _currentFile << ':' << _currentLine << ": warning: " << s << endl;
    }
    else
    {
	cerr << "warning: " << s << endl;
    }
}

void
Parser::warning(const string& s)
{
    warning(s.c_str());
}

int
Parser::parse(FILE* file, bool debug)
{
    yydebug = debug ? 1 : 0;

    assert(!parser);
    parser = this;

    _errors = 0;
    _commands.empty();
    yyin = file;
    assert(yyin);

    _currentFile = "";
    _currentLine = 0;
    _continue = false;
    nextLine();

    int status = yyparse();
    if(_errors)
    {
	status = EXIT_FAILURE;
    }

    parser = 0;
    return status;
}

int
Parser::parse(const std::string& commands, bool debug)
{
    yydebug = debug ? 1 : 0;

    assert(!parser);
    parser = this;

    _errors = 0;
    _commands = commands;
    assert(!_commands.empty());
    yyin = 0;

    _currentFile = "";
    _currentLine = 0;
    _continue = false;
    nextLine();

    int status = yyparse();
    if(_errors)
    {
	status = EXIT_FAILURE;
    }

    parser = 0;
    return status;
}

Parser::Parser(const CommunicatorPtr& communicator, 
	       const AdminSessionPrx& session, 
	       const AdminPrx& admin,
	       bool interactive) :
    _communicator(communicator),
    _session(session),
    _admin(admin),
    _interrupted(false),
    _interactive(interactive)
{
}

void
Parser::exception(const Ice::Exception& ex)
{
    try
    {
	ex.ice_throw();
    }
    catch(const ApplicationNotExistException& ex)
    {
	error("couldn't find application `" + ex.name + "'");
    }
    catch(const NodeNotExistException& ex)
    {
	error("couldn't find node `" + ex.name + "'");
    }
    catch(const ServerNotExistException& ex)
    {
	error("couldn't find server `" + ex.id + "'");
    }
    catch(const AdapterNotExistException& ex)
    {
	error("couldn't find adapter `" + ex.id + "'");
    }
    catch(const ObjectExistsException& ex)
    {
	error("object `" + _communicator->identityToString(ex.id) + "' already exists");
    }
    catch(const DeploymentException& ex)
    {
	ostringstream s;
	s << ex << ":\n" << ex.reason;
	error(s.str());	
    }
    catch(const PatchException& ex)
    {
	if(ex.reasons.size() == 1)
	{
	    ostringstream s;
	    s << ex << ":\n" << ex.reasons[0];
	    error(s.str());
	}
	else
	{
	    patchFailed(ex.reasons);
	}
    }
    catch(const BadSignalException& ex)
    {
	ostringstream s;
	s << ex.reason;
	error(s.str());
    }
    catch(const NodeUnreachableException& ex)
    {
	error("node `" + ex.name + "' couldn't be reached:\n" + ex.reason);
    }
    catch(const AccessDeniedException& ex)
    {
	error("couldn't update the registry, the session from `" + ex.lockUserId + "' is updating the registry");
    }
    catch(const FileNotAvailableException& ex)
    {
	error("couldn't access file:\n" + ex.reason);
    }
    catch(const IceXML::ParserException& ex)
    {
	ostringstream s;
	s << ex;
	error(s.str());
    }
    catch(const Ice::LocalException& ex)
    {
	ostringstream s;
	s << "couldn't reach the IceGrid registry:\n" << ex;
	error(s.str());
    }
    catch(const Ice::Exception& ex)
    {
	ostringstream s;
	s << ex;
	error(s.str());
    }
}
