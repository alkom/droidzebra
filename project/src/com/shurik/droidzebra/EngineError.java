/* Copyright (C) 2012 by Alex Kompel  */
package com.shurik.droidzebra;

public class EngineError extends Exception {

	/**
	 * 
	 */
	private static final long serialVersionUID = 1878908185661232307L;
	String msg;

	 public EngineError()
	  {
	    super();             // call superclass constructor
	    msg = "unknown";
	  }
	  
	  public EngineError(String err)
	  {
	    super(err);     // call super class constructor
	    msg = err;  // save message
	  }
	  
	  public String getError()
	  {
	    return msg;
	  }
}
