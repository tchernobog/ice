// **********************************************************************
//
// Copyright (c) 2001
// Mutable Realms, Inc.
// Huntsville, AL, USA
//
// All Rights Reserved
//
// **********************************************************************

public class Server
{
    private static int
    run(String[] args, Ice.Communicator communicator)
    {
	Ice.Properties properties = communicator.getProperties();
	properties.setProperty("TestAdapter.Endpoints", "default -t 2000");
	properties.setProperty("TestAdapter.AdapterId", "TestAdapter");

        Ice.ObjectAdapter adapter = communicator.createObjectAdapter("TestAdapter");
        Ice.Object object = new TestI(adapter);
        adapter.add(object, Ice.Util.stringToIdentity("test"));
        adapter.activate();
        communicator.waitForShutdown();
        return 0;
    }

    public static void
    main(String[] args)
    {
        int status = 0;
        Ice.Communicator communicator = null;

        try
        {
            communicator = Ice.Util.initialize(args);
            status = run(args, communicator);
        }
        catch (Ice.RuntimeException ex)
        {
            ex.printStackTrace();
            status = 1;
        }

        if (communicator != null)
        {
            try
            {
                communicator.destroy();
            }
            catch (Ice.RuntimeException ex)
            {
                ex.printStackTrace();
                status = 1;
            }
        }

        System.exit(status);
    }
}
