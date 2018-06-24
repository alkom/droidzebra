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

package de.earthlingz.oerszebra;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Paint.FontMetrics;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.View;

import java.util.TreeMap;

public class StatusView extends View {

	abstract private class DrawElement {
		public int mID;
		DrawElement(int id) {
			mID = id;
		}
		abstract void prepareDraw(float scaleW, float scaleH, Paint p);
		abstract public void draw(Canvas c, Paint p);
		abstract public RectF getScreenRectF();
		void invalidate() {
			Rect bounds = new Rect();
			getScreenRectF().roundOut(bounds);
			StatusView.this.invalidate(bounds);
		}
	}

	private class DrawLine extends DrawElement {
		// logical attrs
		private float mXstart;
		private float mYstart;
		private float mXend;
		private float mYend;
		private int mRColor;
		
		// drawable attrs
		private float mScreenXstart;
		private float mScreenYstart;
		private float mScreenXend;
		private float mScreenYend;
		private int mScreenColor;

        public DrawLine(int id, float xstart, float ystart, float xend, float yend, int rcolor) {
            super(id);
            mXstart = xstart;
            mYstart = ystart;
			mXend = xend;
			mYend = yend;
            mRColor = rcolor;
        }

        @Override
		public void prepareDraw(float scaleW, float scaleH, Paint p) {
			mScreenColor = getResources().getColor(mRColor);
			mScreenXstart = mXstart*scaleW;
			mScreenYstart = mYstart*scaleH;
			mScreenXend	= mXend*scaleW;
			mScreenYend = mYend*scaleH;
		}		

		@Override public void draw(Canvas c, Paint p) {
			p.setColor(mScreenColor);
			c.drawLine(mScreenXstart, mScreenYstart, mScreenXend, mScreenYend, p);
		}
		
		@Override
		public RectF getScreenRectF() {
			return new RectF(mXstart, mYstart, mXend, mYend);
		}
	}

	private class DrawText extends DrawElement {
		public static final int
			FLAG_FIT = 0x0001,
			FLAG_TRUNCATE = 0x0002;

		// logical attrs
		private float mXstart;
		private float mYstart;
		private float mXend;
		private float mYend;
		private String mText;
		private int mRColor;
		private Paint.Align mAlign;
		private int mFlags;
		
		// screen attributes
		private RectF mScreenBounds;
		private int mScreenColor;
		private String mScreenText;
		private float mScreenTextSize;
		
		
		public DrawText(int id, float xstart, float ystart, float xend, float yend, String text, int rcolor, Paint.Align align, int flags) {
			super(id);
			mXstart = xstart;
			mYstart = ystart;
			mXend = xend;
			mYend = yend;
			mText = text;
			mRColor = rcolor;
			mAlign = align;
			mFlags = flags;
		}

		public void setText(String text) {
			mText = text;
		}

		@Override
		public void prepareDraw(float scaleW, float scaleH, Paint p) {
			mScreenBounds = new RectF(mXstart*scaleW, mYstart*scaleH, mXend*scaleW, mYend*scaleH);
			mScreenColor = getResources().getColor(mRColor);
				
			mScreenTextSize = mScreenBounds.height()*0.8f; 
			mScreenText = mText;
			p.setTextSize(mScreenTextSize);
			
			if( (mFlags & FLAG_TRUNCATE)>0 ) {
				float width = mScreenBounds.width(); 
				int numch = mText.length();
				boolean truncate = false;
				int i;
				float[] cw = new float[numch];

				p.getTextWidths(mText, cw);
				for(i=0; i<numch; i++) {
					width -= cw[i];
					if(width<0) {
						truncate = true;
						break;
					}
				}
				if( truncate && i>3 ) {
					mScreenText = mText.substring(0, i-3);
					mScreenText += "...";
				}  else {
					mScreenText = mText.substring(0, i);
				}
			} else if( (mFlags & FLAG_FIT)>0 ) {
				Rect bounds = new Rect();
				p.getTextBounds(mText, 0, mText.length(), bounds);
				if( mScreenBounds.width()/bounds.width()<0.8f ) {
					mScreenTextSize = mScreenBounds.height()*mScreenBounds.width()/bounds.width();
				}
			}
		}
		
		@Override public void draw(Canvas c, Paint p) {
			float x = mScreenBounds.left;
			p.setTextSize(mScreenTextSize);
			p.setColor(mScreenColor);
			FontMetrics fontMetrics = p.getFontMetrics();
			
			if(mAlign == Paint.Align.LEFT || mScreenText.endsWith("...")) {
				x = mScreenBounds.left;
				p.setTextAlign(Paint.Align.LEFT);
			} else if(mAlign == Paint.Align.CENTER) {
				x = mScreenBounds.centerX();
				p.setTextAlign(Paint.Align.CENTER);
			} else if(mAlign == Paint.Align.RIGHT) {
				x = mScreenBounds.right;
				p.setTextAlign(Paint.Align.RIGHT);
			}
			c.drawText(mScreenText, x, mScreenBounds.centerY() - (fontMetrics.ascent+fontMetrics.descent)/2, p);
		}

		@Override
		public RectF getScreenRectF() {
			return mScreenBounds;
		}
	}

	public static final int
		ID_NONE = -1,
		ID_SCORE_BLACK = 1,
		ID_SCORE_WHITE = 2,
		ID_SCORE_SKILL = 3,
		ID_SCORELINE_NUM_1 = 10,
		ID_SCORELINE_NUM_2 = 11,
		ID_SCORELINE_NUM_3 = 12,
		ID_SCORELINE_NUM_4 = 13,
		ID_SCORELINE_WHITE_1 = 20,
		ID_SCORELINE_WHITE_2 = 21,
		ID_SCORELINE_WHITE_3 = 22,
		ID_SCORELINE_WHITE_4 = 23,
		ID_SCORELINE_BLACK_1 = 30,
		ID_SCORELINE_BLACK_2 = 31,
		ID_SCORELINE_BLACK_3 = 32,
		ID_SCORELINE_BLACK_4 = 33,
		ID_STATUS_OPENING = 50,
		ID_STATUS_PV = 51,
		ID_STATUS_EVAL = 52
		;
	
	private static final float
		HSF = 0.8f, // horizontal scale factor between score box and moves table
	
		STATUS_POS_Y = 0.8f,
		
		SCORE_BOX_START_Y = 0.18f,
		SCORE_BOX_END_Y = 0.76f,
		
		SCORE_BOX_NUM_START_X = 0.02f*HSF,
		SCORE_BOX_NUM_END_X = 0.10f*HSF,

		SCORE_BOX_BLACK_START_X = 0.16f*HSF,
		SCORE_BOX_BLACK_END_X = 0.28f*HSF,

		SCORE_BOX_WHITE_START_X = 0.32f*HSF,
		SCORE_BOX_WHITE_END_X = 0.46f*HSF
		;
		
	private  DrawElement[] mLayout = {
            new DrawLine(ID_NONE, 0.0f, 0.0f, 1.0f, 0.0f, R.color.statuslinecolor),
            new DrawLine(ID_NONE, 0.0f, 1.0f, 1.0f, 1.0f, R.color.statuslinecolor),
            new DrawLine(ID_NONE, 0.0f, 0.0f, 0.0f, 1.0f, R.color.statuslinecolor),
            new DrawLine(ID_NONE, 1.0f, 0.0f, 1.0f, 1.0f, R.color.statuslinecolor),

            new DrawLine(ID_NONE, 0.5f * HSF, 0.0f, 0.5f * HSF, STATUS_POS_Y, R.color.statuslinecolor),
            new DrawLine(ID_NONE, 0.0f, STATUS_POS_Y, 1.0f, STATUS_POS_Y, R.color.statuslinecolor),

		// moves panel
            new DrawLine(ID_NONE, 0.12f * HSF, SCORE_BOX_START_Y, 0.48f * HSF, SCORE_BOX_START_Y, R.color.statuslinecolor),
            new DrawLine(ID_NONE, 0.30f * HSF, SCORE_BOX_START_Y, 0.30f * HSF, SCORE_BOX_END_Y, R.color.statuslinecolor),
            new DrawText(ID_NONE, 0.12f * HSF, 0.02f, 0.30f * HSF, SCORE_BOX_START_Y - 0.02f, "B", R.color.statustextcolor, Paint.Align.CENTER, DrawText.FLAG_FIT),
            new DrawText(ID_NONE, 0.30f*HSF, 0.02f, 0.48f*HSF, SCORE_BOX_START_Y-0.02f, "W", R.color.statustextcolor, Paint.Align.CENTER, DrawText.FLAG_FIT),
		
		new DrawText(ID_SCORELINE_NUM_1, SCORE_BOX_NUM_START_X, SCORE_BOX_START_Y+0*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, SCORE_BOX_NUM_END_X, SCORE_BOX_START_Y+1*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, "12", R.color.statustextcolor, Paint.Align.RIGHT, 0),
		new DrawText(ID_SCORELINE_NUM_2, SCORE_BOX_NUM_START_X, SCORE_BOX_START_Y+1*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, SCORE_BOX_NUM_END_X, SCORE_BOX_START_Y+2*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, "13", R.color.statustextcolor, Paint.Align.RIGHT, 0),
		new DrawText(ID_SCORELINE_NUM_3, SCORE_BOX_NUM_START_X, SCORE_BOX_START_Y+2*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, SCORE_BOX_NUM_END_X, SCORE_BOX_START_Y+3*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, "14", R.color.statustextcolor, Paint.Align.RIGHT, 0),
		new DrawText(ID_SCORELINE_NUM_4, SCORE_BOX_NUM_START_X, SCORE_BOX_START_Y+3*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, SCORE_BOX_NUM_END_X, SCORE_BOX_START_Y+4*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, "15", R.color.statustextcolor, Paint.Align.RIGHT, 0),

		new DrawText(ID_SCORELINE_BLACK_1, SCORE_BOX_BLACK_START_X, SCORE_BOX_START_Y+0*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, SCORE_BOX_BLACK_END_X, SCORE_BOX_START_Y+1*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, "a1", R.color.statustextcolor, Paint.Align.CENTER, 0),
		new DrawText(ID_SCORELINE_BLACK_2, SCORE_BOX_BLACK_START_X, SCORE_BOX_START_Y+1*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, SCORE_BOX_BLACK_END_X, SCORE_BOX_START_Y+2*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, "d3", R.color.statustextcolor, Paint.Align.CENTER, 0),
		new DrawText(ID_SCORELINE_BLACK_3, SCORE_BOX_BLACK_START_X, SCORE_BOX_START_Y+2*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, SCORE_BOX_BLACK_END_X, SCORE_BOX_START_Y+3*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, "g8", R.color.statustextcolor, Paint.Align.CENTER, 0),
		new DrawText(ID_SCORELINE_BLACK_4, SCORE_BOX_BLACK_START_X, SCORE_BOX_START_Y+3*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, SCORE_BOX_BLACK_END_X, SCORE_BOX_START_Y+4*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, "d7", R.color.statustextcolor, Paint.Align.CENTER, 0),

		new DrawText(ID_SCORELINE_WHITE_1, SCORE_BOX_WHITE_START_X, SCORE_BOX_START_Y+0*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, SCORE_BOX_WHITE_END_X, SCORE_BOX_START_Y+1*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, "c4", R.color.statustextcolor, Paint.Align.CENTER, 0),
		new DrawText(ID_SCORELINE_WHITE_2, SCORE_BOX_WHITE_START_X, SCORE_BOX_START_Y+1*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, SCORE_BOX_WHITE_END_X, SCORE_BOX_START_Y+2*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, "d3", R.color.statustextcolor, Paint.Align.CENTER, 0),
		new DrawText(ID_SCORELINE_WHITE_3, SCORE_BOX_WHITE_START_X, SCORE_BOX_START_Y+2*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, SCORE_BOX_WHITE_END_X, SCORE_BOX_START_Y+3*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, "f5", R.color.statustextcolor, Paint.Align.CENTER, 0),
		new DrawText(ID_SCORELINE_WHITE_4, SCORE_BOX_WHITE_START_X, SCORE_BOX_START_Y+3*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, SCORE_BOX_WHITE_END_X, SCORE_BOX_START_Y+4*(SCORE_BOX_END_Y-SCORE_BOX_START_Y)/4, "f9", R.color.statustextcolor, Paint.Align.CENTER, 0),

		// score panel
		new DrawText(ID_SCORE_BLACK, 0.44f, 0.10f, 0.67f, STATUS_POS_Y-0.30f, "34", R.color.black, Paint.Align.RIGHT, 0),
		new DrawText(ID_NONE, 0.67f, 0.20f, 0.73f, STATUS_POS_Y-0.40f, ":", R.color.statustextcolor, Paint.Align.CENTER, DrawText.FLAG_FIT),
		new DrawText(ID_SCORE_WHITE, 0.73f, 0.10f, 0.96f, STATUS_POS_Y-0.30f, "21", R.color.white, Paint.Align.LEFT, 0),
		new DrawText(ID_SCORE_SKILL, 0.48f, STATUS_POS_Y-0.25f, 0.92f, STATUS_POS_Y-0.15f, "Mid: 8  Exact: 16  W/L: 18", R.color.white, Paint.Align.CENTER, DrawText.FLAG_FIT),
		
		// status panel
            new DrawLine(ID_NONE, 0.40f, STATUS_POS_Y, 0.40f, 1.0f, R.color.statuslinecolor),
            new DrawLine(ID_NONE, 0.80f, STATUS_POS_Y, 0.80f, 1.0f, R.color.statuslinecolor),
            new DrawText(ID_STATUS_OPENING, 0.0f, STATUS_POS_Y + 0.04f, 0.40f, 1.0f - 0.04f, "Some other opening", R.color.statustextcolor, Paint.Align.CENTER, DrawText.FLAG_TRUNCATE),
            new DrawText(ID_STATUS_PV, 0.40f, STATUS_POS_Y+0.04f, 0.80f, 1.0f-0.04f, "a1 b1 -- c2", R.color.statustextcolor, Paint.Align.CENTER, DrawText.FLAG_TRUNCATE),
		new DrawText(ID_STATUS_EVAL, 0.80f, STATUS_POS_Y+0.04f, 1.0f, 1.0f-0.04f, "+5.4", R.color.statustextcolor, Paint.Align.CENTER, DrawText.FLAG_TRUNCATE),
	};
	private TreeMap<Integer, DrawElement> mDrawTextIDMap = new TreeMap<Integer, DrawElement>();
	
	private Paint mPaint;
	
	public StatusView(Context context, AttributeSet attrs) {
		super(context, attrs);
		setFocusable(false);

		mPaint = new Paint();
		mPaint.setAntiAlias(true);
		mPaint.setStyle(Paint.Style.FILL);
		mPaint.setStrokeWidth(Math.max(1f, TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 0.5f, getResources().getDisplayMetrics())));
		mPaint.setTypeface(Typeface.SERIF);
		
		for(DrawElement elem:mLayout) {
			if( elem.mID!=ID_NONE && DrawText.class.isInstance(elem))
				mDrawTextIDMap.put(elem.mID, elem);
		}
	}
	
	public void setTextForID(int id, String text) {
		DrawText de = (DrawText)mDrawTextIDMap.get(id);
		if( de!=null ) {
			de.setText(text);
			if( getWidth()>0 && getHeight()>0 ) {
				de.prepareDraw(getWidth()-1, getHeight()-1, mPaint);
				post(this::invalidate);
			}
		}
	}

	public void clear() {
		String blank = new String();
		setTextForID(ID_SCORE_BLACK, blank);
		setTextForID(ID_SCORE_WHITE,  blank);
		setTextForID(ID_SCORELINE_NUM_1, "1");
		setTextForID(ID_SCORELINE_NUM_2, "2");
		setTextForID(ID_SCORELINE_NUM_3, "3");
		setTextForID(ID_SCORELINE_NUM_4, "4");
		setTextForID(ID_SCORELINE_WHITE_1, blank);
		setTextForID(ID_SCORELINE_WHITE_2, blank);
		setTextForID(ID_SCORELINE_WHITE_3, blank);
		setTextForID(ID_SCORELINE_WHITE_4, blank);
		setTextForID(ID_SCORELINE_BLACK_1, blank);
		setTextForID(ID_SCORELINE_BLACK_2, blank);
		setTextForID(ID_SCORELINE_BLACK_3, blank);
		setTextForID(ID_SCORELINE_BLACK_4, blank);
		setTextForID(ID_STATUS_OPENING, blank);
		setTextForID(ID_STATUS_PV, blank);
		setTextForID(ID_STATUS_EVAL, blank);
	}

	@Override
	protected void onDraw(Canvas canvas) {
		super.onDraw(canvas);
		
		if( getWidth()==0 || getHeight()==0 ) return;
		
		for(DrawElement elem:mLayout) {
			elem.draw(canvas, mPaint);
		}
	}
	
	private void recalculateLayout(float width, float height)
	{
		for(DrawElement elem:mLayout) {
			elem.prepareDraw(width, height, mPaint);
		}
	}
	
	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		super.onMeasure(widthMeasureSpec, heightMeasureSpec);
	}

	@Override
	protected void onSizeChanged(int w, int h, int oldw, int oldh) {
		if( w>0 && h>0 ) {
			recalculateLayout(w-1, h-1);
			invalidate();
		}
		super.onSizeChanged(w, h, oldw, oldh);
	}

	
}
