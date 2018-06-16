/* Copyright (C) 2010 by Alex Kompel  */
/* This file is part of DroidZebra.

    DroidZebra is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    DroidZebra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DroidZebra.  If not, see <http://www.gnu.org/licenses/>
*/

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
