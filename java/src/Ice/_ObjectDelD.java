// **********************************************************************
//
// Copyright (c) 2001
// Mutable Realms, Inc.
// Huntsville, AL, USA
//
// All Rights Reserved
//
// **********************************************************************

package Ice;

public class _ObjectDelD implements _ObjectDel
{
    public boolean
    ice_isA(String __id, java.util.Map __context)
        throws LocationForward, IceInternal.NonRepeatable
    {
        Current __current = new Current();
        __initCurrent(__current, "ice_isA", OperationMode.Nonmutating, __context);
        while(true)
        {
            IceInternal.Direct __direct = new IceInternal.Direct(__adapter, __current);
            try
            {
                try
                {
                    return __direct.facetServant().ice_isA(__id, __current);
                }
                catch(RuntimeException ex)
                {
                    UnknownRuntimeException e = new UnknownRuntimeException();
                    e.initCause(ex);
                    throw e;
                }
                catch(java.lang.RuntimeException ex)
                {
                    UnknownException e = new UnknownException();
                    e.initCause(ex);
                    throw e;
                }
            }
            finally
            {
                __direct.destroy();
            }
        }
    }

    public void
    ice_ping(java.util.Map __context)
        throws LocationForward, IceInternal.NonRepeatable
    {
        Current __current = new Current();
        __initCurrent(__current, "ice_ping", OperationMode.Nonmutating, __context);
        while(true)
        {
            IceInternal.Direct __direct = new IceInternal.Direct(__adapter, __current);
            try
            {
                try
                {
                    __direct.facetServant().ice_ping(__current);
                    return;
                }
                catch(RuntimeException ex)
                {
                    UnknownRuntimeException e = new UnknownRuntimeException();
                    e.initCause(ex);
                    throw e;
                }
                catch(java.lang.RuntimeException ex)
                {
                    UnknownException e = new UnknownException();
                    e.initCause(ex);
                    throw e;
                }
            }
            finally
            {
                __direct.destroy();
            }
        }
    }

    public String[]
    ice_ids(java.util.Map __context)
        throws LocationForward, IceInternal.NonRepeatable
    {
        Current __current = new Current();
        __initCurrent(__current, "ice_ids", OperationMode.Nonmutating, __context);
        while(true)
        {
            IceInternal.Direct __direct = new IceInternal.Direct(__adapter, __current);
            try
            {
                try
                {
                    return __direct.facetServant().ice_ids(__current);
                }
                catch(RuntimeException ex)
                {
                    UnknownRuntimeException e = new UnknownRuntimeException();
                    e.initCause(ex);
                    throw e;
                }
                catch(java.lang.RuntimeException ex)
                {
                    UnknownException e = new UnknownException();
                    e.initCause(ex);
                    throw e;
                }
            }
            finally
            {
                __direct.destroy();
            }
        }
    }

    public String
    ice_id(java.util.Map __context)
        throws LocationForward, IceInternal.NonRepeatable
    {
        Current __current = new Current();
        __initCurrent(__current, "ice_id", OperationMode.Nonmutating, __context);
        while(true)
        {
            IceInternal.Direct __direct = new IceInternal.Direct(__adapter, __current);
            try
            {
                try
                {
                    return __direct.facetServant().ice_id(__current);
                }
                catch(RuntimeException ex)
                {
                    UnknownRuntimeException e = new UnknownRuntimeException();
                    e.initCause(ex);
                    throw e;
                }
                catch(java.lang.RuntimeException ex)
                {
                    UnknownException e = new UnknownException();
                    e.initCause(ex);
                    throw e;
                }
            }
            finally
            {
                __direct.destroy();
            }
        }
    }

    public String[]
    ice_facets(java.util.Map __context)
        throws LocationForward, IceInternal.NonRepeatable
    {
        Current __current = new Current();
        __initCurrent(__current, "ice_facets", OperationMode.Nonmutating, __context);
        while(true)
        {
            IceInternal.Direct __direct = new IceInternal.Direct(__adapter, __current);
            try
            {
                try
                {
                    return __direct.facetServant().ice_facets(__current);
                }
                catch(RuntimeException ex)
                {
                    UnknownRuntimeException e = new UnknownRuntimeException();
                    e.initCause(ex);
                    throw e;
                }
                catch(java.lang.RuntimeException ex)
                {
                    UnknownException e = new UnknownException();
                    e.initCause(ex);
                    throw e;
                }
            }
            finally
            {
                __direct.destroy();
            }
        }
    }

    public boolean
    ice_invoke(String operation, Ice.OperationMode mode, byte[] inParams, ByteSeqHolder outParams,
               java.util.Map context)
        throws LocationForward, IceInternal.NonRepeatable
    {
        Current current = new Current();
        __initCurrent(current, operation, mode, context);
        while(true)
        {
            IceInternal.Direct __direct = new IceInternal.Direct(__adapter, current);
            try
            {
                Blobject __servant = null;
                try
                {
                    __servant = (Blobject)__direct.facetServant();
                }
                catch(ClassCastException ex)
                {
                    OperationNotExistException opEx = new OperationNotExistException();
		    opEx.operation = current.operation;
		    throw opEx;
                }
                try
                {
                    return __servant.ice_invoke(inParams, outParams, current);
                }
                catch(RuntimeException ex)
                {
                    UnknownRuntimeException e = new UnknownRuntimeException();
                    e.initCause(ex);
                    throw e;
                }
                catch(java.lang.RuntimeException ex)
                {
                    UnknownException e = new UnknownException();
                    e.initCause(ex);
                    throw e;
                }
            }
            finally
            {
                __direct.destroy();
            }
        }
    }

    public void
    ice_flush()
    {
        // Nothing to do for direct delegates
    }

    //
    // Only for use by ObjectPrx
    //
    final void
    __copyFrom(_ObjectDelD from)
    {
        //
        // No need to synchronize "from", as the delegate is immutable
        // after creation.
        //

        //
        // No need to synchronize, as this operation is only called
        // upon initialization.
        //

        __reference = from.__reference;
        __adapter = from.__adapter;
    }

    protected Ice.ObjectAdapter __adapter;
    protected IceInternal.Reference __reference;

    protected final void
    __initCurrent(Current current, String op, Ice.OperationMode mode, java.util.Map context)
    {
        current.id = __reference.identity;
        current.facet = __reference.facet;
        current.operation = op;
        current.mode = mode;
        current.ctx = context;
    }

    public void
    setup(IceInternal.Reference ref, Ice.ObjectAdapter adapter)
    {
        //
        // No need to synchronize, as this operation is only called
        // upon initialization.
        //
        __reference = ref;
        __adapter = adapter;
    }
}
