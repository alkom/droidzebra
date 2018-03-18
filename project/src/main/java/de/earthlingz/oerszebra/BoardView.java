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
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Paint.FontMetrics;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.os.CountDownTimer;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import com.shurik.droidzebra.CandidateMove;
import com.shurik.droidzebra.EngineError;
import com.shurik.droidzebra.InvalidMove;
import com.shurik.droidzebra.Move;
import com.shurik.droidzebra.ZebraEngine;
//import android.util.Log;

public class BoardView extends View {

	private float lineWidth = 1;
	private float gridCirclesRadius = 3;
	
	private int mColorHelpersValid;
	private int mColorHelpersInvalid;
	private int mColorSelectionValid;
	private int mColorSelectionInvalid;
	private int mColorLine;
	private int mColorNumbers;
	private int mColorEvals;
	private int mColorEvalsBest;
	
	private DroidZebra mDroidZebra = null;
	
	private float mSizeX = 0;
	private float mSizeY = 0;
	private float mSizeCell = 0;
	private RectF mBoardRect = null;
	private Paint mPaint = null;
	private RectF mTempRect = null;
	private FontMetrics mFontMetrics = null; 
	private Paint mPaintEvalText = null;
	private FontMetrics mEvalFontMetrics = null;

	private BitmapShader mShaderV = null;
	private BitmapShader mShaderH = null;
	private Path mPath = null;
	
	private Move mMoveSelection = new Move(0,0);
	private boolean mShowSelection = false; // highlight selection rectangle
	private boolean mShowSelectionHelpers = false ;// highlight row/column
	
	private CountDownTimer mAnimationTimer = null;
	private boolean mIsAnimationRunning = false;
	private double mAnimationProgress = 0;
	
	public BoardView(Context context) {
		super(context);
		initBoardView();
	}
		
	public BoardView(Context context, AttributeSet attrs) {
		super(context,attrs);
		initBoardView();
	}
	
	private void initBoardView() {
		if(DroidZebra.class.isInstance(getContext()))
			setDroidZebra((DroidZebra)getContext());

		Resources r = getResources();
		setFocusable(true); // make sure we get key events

		mColorHelpersValid = r.getColor(R.color.board_color_helpers_valid);
		mColorHelpersInvalid = r.getColor(R.color.board_color_helpers_invalid);
		mColorSelectionValid = r.getColor(R.color.board_color_selection_valid);
		mColorSelectionInvalid = r.getColor(R.color.board_color_selection_invalid);
		mColorEvals = Color.YELLOW;
		mColorEvalsBest = Color.CYAN;
		mColorLine = r.getColor(R.color.board_line);
		mColorNumbers = r.getColor(R.color.board_numbers);
		
		Bitmap woodtrim = BitmapFactory.decodeResource(r, R.drawable.woodtrim);
		mShaderV = new BitmapShader(woodtrim, Shader.TileMode.REPEAT, Shader.TileMode.REPEAT);
		mShaderH = new BitmapShader(woodtrim, Shader.TileMode.REPEAT, Shader.TileMode.REPEAT);
		Matrix m = new Matrix();
		m.setRotate(90);
		mShaderH.setLocalMatrix(m);
		mPath = new Path();

		mPaint = new Paint();
		mPaintEvalText = new Paint();
		mBoardRect = new RectF();
		mTempRect = new RectF();
		
		int animationDelay = 100;
		if( !isInEditMode() )
			animationDelay = getDroidZebra().mSettingAnimationDelay;
	    mAnimationProgress = 0;		
		mAnimationTimer = new CountDownTimer(animationDelay, animationDelay/10) {

	        public void onTick(long millisUntilFinished) {
	        	mAnimationProgress = 1.0-(double)millisUntilFinished/getDroidZebra().mSettingAnimationDelay;
	        	invalidate();
	        }

	        public void onFinish() {
	        	mIsAnimationRunning = false;
	        	invalidate();
	        }
	    };
	}

	public void setDroidZebra(DroidZebra activity) {
		mDroidZebra = activity;
	}
	
	public DroidZebra getDroidZebra() {
		return mDroidZebra;
	}
	
	public RectF getCellRect(int bx, int by) {
		return new RectF(
				mBoardRect.left+bx*mSizeCell, 
				mBoardRect.top+by*mSizeCell, 
				mBoardRect.left+bx*mSizeCell+mSizeCell-1, 
				mBoardRect.top+by*mSizeCell+mSizeCell-1
			);
	}

	public Move getMoveFromCoord(float x, float y) throws InvalidMove {
		int bx = (int)Math.floor((x - mBoardRect.left)/mSizeCell);
		int by = (int)Math.floor((y - mBoardRect.top)/mSizeCell);
		if (bx < 0 || bx >= BoardState.boardSize || by < 0 || by >= BoardState.boardSize) {
			throw new InvalidMove();
		}
		return new Move(bx, by);
	}
	
	@Override
	protected void onDraw(Canvas canvas) {

		// draw borders
		mPaint.setShader(mShaderV);

		mPath.reset(); 
		mPath.moveTo(0, 0); 
		mPath.lineTo(0, mSizeY); 
		mPath.lineTo(mBoardRect.left, mBoardRect.bottom);
		mPath.lineTo(mBoardRect.left, mBoardRect.top);
		mPath.lineTo(0, 0);
		canvas.drawPath(mPath, mPaint);

		mPath.reset(); 
		mPath.moveTo(mSizeX, 0); 
		mPath.lineTo(mSizeX, mSizeY); 
		mPath.lineTo(mBoardRect.right, mBoardRect.bottom);
		mPath.lineTo(mBoardRect.right, mBoardRect.top);
		mPath.lineTo(mSizeX, 0);
		canvas.drawPath(mPath, mPaint);        
		
		mPaint.setShader(mShaderH);

		mPath.reset(); 
		mPath.moveTo(0, 0); 
		mPath.lineTo(mSizeX, 0); 
		mPath.lineTo(mBoardRect.right, mBoardRect.top);
		mPath.lineTo(mBoardRect.left, mBoardRect.top);
		mPath.lineTo(0, 0);
		canvas.drawPath(mPath, mPaint);

		mPath.reset(); 
		mPath.moveTo(0, mSizeY); 
		mPath.lineTo(mSizeX, mSizeY); 
		mPath.lineTo(mBoardRect.right, mBoardRect.bottom);
		mPath.lineTo(mBoardRect.left, mBoardRect.bottom);
		mPath.lineTo(0, mSizeY);
		canvas.drawPath(mPath, mPaint);        

		mPaint.setShader(null);
		
		// draw the board
		mPaint.setStrokeWidth(lineWidth);
		int boardSize = BoardState.boardSize;
		for (int i = 0; i <= boardSize; i++) {
			mPaint.setColor(mColorLine);
			canvas.drawLine(mBoardRect.left + i * mSizeCell, mBoardRect.top, mBoardRect.left + i * mSizeCell, mBoardRect.top + mSizeCell * boardSize, mPaint);
			canvas.drawLine(mBoardRect.left, mBoardRect.top + i * mSizeCell, mBoardRect.left + mSizeCell * boardSize, mBoardRect.top + i * mSizeCell, mPaint);
		}
		canvas.drawCircle(mBoardRect.left+2*mSizeCell, mBoardRect.top+2*mSizeCell, gridCirclesRadius, mPaint);
		canvas.drawCircle(mBoardRect.left+2*mSizeCell, mBoardRect.top+6*mSizeCell, gridCirclesRadius, mPaint);
		canvas.drawCircle(mBoardRect.left+6*mSizeCell, mBoardRect.top+2*mSizeCell, gridCirclesRadius, mPaint);
		canvas.drawCircle(mBoardRect.left+6*mSizeCell, mBoardRect.top+6*mSizeCell, gridCirclesRadius, mPaint);

		// draw guides
		for (int i = 0; i < boardSize; i++) {
			mPaint.setTextSize(mSizeCell*0.3f);
			mPaint.setColor(Color.BLACK);
			canvas.drawText(String.valueOf(i+1), mBoardRect.left/2 + 1, mBoardRect.top + i*mSizeCell + mSizeCell/2 - (mFontMetrics.ascent+mFontMetrics.descent)/2 + 1, mPaint);
			canvas.drawText(Character.toString((char) ('A'+i)), mBoardRect.left + i*mSizeCell + mSizeCell/2 + 1, mBoardRect.top/2-(mFontMetrics.ascent+mFontMetrics.descent)/2 + 1, mPaint);
			mPaint.setColor(mColorNumbers);
			canvas.drawText(String.valueOf(i+1), mBoardRect.left/2, mBoardRect.top + i*mSizeCell + mSizeCell/2 - (mFontMetrics.ascent+mFontMetrics.descent)/2, mPaint);
			canvas.drawText(Character.toString((char) ('A'+i)), mBoardRect.left + i*mSizeCell + mSizeCell/2, mBoardRect.top/2-(mFontMetrics.ascent+mFontMetrics.descent)/2, mPaint);
		}

		// draw helpers for move selector
		if( mMoveSelection != null ) {
			if( mShowSelectionHelpers ) {
				if (mDroidZebra.getState().isValidMove(mMoveSelection))
					mPaint.setColor(mColorHelpersValid);
				else
					mPaint.setColor(mColorHelpersInvalid);

				canvas.drawRect(
	        			mBoardRect.left+mMoveSelection.getX()*mSizeCell, 
	        			mBoardRect.top, 
	        			mBoardRect.left+(mMoveSelection.getX()+1)*mSizeCell, 
	        			mBoardRect.bottom,
	        			mPaint
	        		);        	
	        	canvas.drawRect(
	        			mBoardRect.left, 
	        			mBoardRect.top+mMoveSelection.getY()*mSizeCell, 
	        			mBoardRect.right, 
	        			mBoardRect.top+(mMoveSelection.getY()+1)*mSizeCell,
	        			mPaint
	        		);
			} else if( mShowSelection ) {
				if (mDroidZebra.getState().isValidMove(mMoveSelection))
					mPaint.setColor(mColorSelectionValid);
				else
					mPaint.setColor(mColorSelectionInvalid);

				canvas.drawRect(
	        			mBoardRect.left+mMoveSelection.getX()*mSizeCell, 
	        			mBoardRect.top+mMoveSelection.getY()*mSizeCell,
	        			mBoardRect.left+(mMoveSelection.getX()+1)*mSizeCell, 
	        			mBoardRect.top+(mMoveSelection.getY()+1)*mSizeCell,
	        			mPaint
	        		);        	
			}
		}
		
		// if we are not playing we are done (designer only)
		if( getDroidZebra()==null ) 
			return;
		
		// draw moves
		float oval_x, oval_y;
		float circle_r = mSizeCell/2-lineWidth*2;
		int circle_color;
		float oval_adjustment = (float)Math.abs(circle_r*Math.cos(Math.PI*mAnimationProgress));
		for (int i = 0; i < boardSize; i++) {
			for (int j = 0; j < boardSize; j++) {
				if (getDroidZebra().getBoard()[i][j].getState() == ZebraEngine.PLAYER_EMPTY)
					continue;
				if(getDroidZebra().getBoard()[i][j].getState()==ZebraEngine.PLAYER_BLACK)
					circle_color = Color.BLACK;
				else
					circle_color = Color.WHITE;
				if(mIsAnimationRunning && getDroidZebra().getBoard()[i][j].isFlipped() ) {
					oval_x = mBoardRect.left+i*mSizeCell+mSizeCell/2; 
					oval_y = mBoardRect.top+j*mSizeCell+mSizeCell/2;
					mTempRect.set(
							oval_x - oval_adjustment,
							oval_y - circle_r, 
							oval_x + oval_adjustment, 
							oval_y + circle_r
							);
					// swap circle color if in animation is less than 50% done (flipping black->white and vice versa)
					if(mAnimationProgress<0.5) 
						if( circle_color==Color.BLACK )
							circle_color = Color.WHITE;
						else
							circle_color = Color.BLACK;
					mPaint.setColor(circle_color);
					canvas.drawOval(mTempRect, mPaint);
				} else {
					mPaint.setColor(circle_color);
					canvas.drawCircle(
							mBoardRect.left+i*mSizeCell+mSizeCell/2, 
							mBoardRect.top+j*mSizeCell+mSizeCell/2, 
							circle_r, 
							mPaint
						);
				}
			}
		}

		// draw evals if in practive mode
		if( (getDroidZebra().mSettingDisplayMoves || getDroidZebra().evalsDisplayEnabled() )
			&& getDroidZebra().getCandidateMoves()!=null ) {
			mPaint.setStrokeWidth(lineWidth*2);
			float lineLength = mSizeCell/4;
			for( CandidateMove m : getDroidZebra().getCandidateMoves() ) {
				RectF cr = getCellRect(m.mMove.getX(), m.mMove.getY());
				if(m.mHasEval && getDroidZebra().evalsDisplayEnabled()) {
					if(m.mBest)
						mPaintEvalText.setColor(mColorEvalsBest);
					else
						mPaintEvalText.setColor(mColorEvals);
					canvas.drawText(m.mEvalShort, cr.centerX(), cr.centerY() - (mEvalFontMetrics.ascent+mEvalFontMetrics.descent)/2, mPaintEvalText);
				} else {
					float pts[] =
					{ 
						cr.centerX() - lineLength/2,
						cr.centerY() - lineLength/2,
						cr.centerX() + lineLength/2,
						cr.centerY() + lineLength/2,
						cr.centerX() + lineLength/2,
						cr.centerY() - lineLength/2,
						cr.centerX() - lineLength/2,
						cr.centerY() + lineLength/2,
					};
					mPaint.setColor(Color.RED);
					canvas.drawLines(pts, 0, 8, mPaint);
				}
			}
		}

		// draw last move marker
		if( getDroidZebra().mSettingDisplayLastMove && getDroidZebra().getLastMove()!=null ) {
			Move lm = getDroidZebra().getLastMove();
			RectF cellRT = getCellRect(lm.getX(), lm.getY());
			mPaint.setColor(Color.BLUE);
			canvas.drawCircle(cellRT.left+mSizeCell/10, cellRT.bottom-mSizeCell/10, mSizeCell/10, mPaint);			
		}
	}

	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		super.onMeasure(widthMeasureSpec, heightMeasureSpec);

		mSizeX = mSizeY = Math.min(getMeasuredWidth(), getMeasuredHeight());
		mSizeCell = Math.min(mSizeX / (BoardState.boardSize + 1), mSizeY / (BoardState.boardSize + 1));
		lineWidth = Math.max(1f, mSizeCell/40f);
		gridCirclesRadius = Math.max(3f, mSizeCell/13f);
		mBoardRect.set(
				mSizeX - mSizeCell / 2 - mSizeCell * BoardState.boardSize,
				mSizeY - mSizeCell / 2 - mSizeCell * BoardState.boardSize,
				mSizeX-mSizeCell/2,
				mSizeY-mSizeCell/2
			);

		mPaint.reset();
		mPaint.setStyle(Paint.Style.FILL);
		Typeface font = Typeface.create(Typeface.SANS_SERIF, Typeface.BOLD);
		mPaint.setTypeface( font );
		mPaint.setAntiAlias(true); 
		mPaint.setTextAlign(Paint.Align.CENTER);
		mPaint.setTextScaleX(1.0f);
		mPaint.setTextSize(mSizeCell*0.3f);
		mPaint.setStrokeWidth(lineWidth);
		mFontMetrics = mPaint.getFontMetrics();

		mPaintEvalText.reset();
		mPaintEvalText.setStyle(Paint.Style.FILL);
		font = Typeface.create(Typeface.SANS_SERIF, Typeface.NORMAL);
		mPaintEvalText.setTypeface( font );
		mPaintEvalText.setAntiAlias(true); 
		mPaintEvalText.setTextAlign(Paint.Align.CENTER);
		mPaintEvalText.setTextScaleX(1.0f);
		mPaintEvalText.setTextSize(mSizeCell*0.5f);
		mPaintEvalText.setStrokeWidth(lineWidth);
		mEvalFontMetrics = mPaintEvalText.getFontMetrics();

		setMeasuredDimension((int)mSizeX, (int)mSizeY);
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent msg) {
		// Log.d("BoardView", String.format("onKeyDown: %d", keyCode));
		if(mIsAnimationRunning) return false;
		
		int newMX = mMoveSelection.getX();
		int newMY = mMoveSelection.getY();
		
		newMX = mMoveSelection.getX();
		newMY = mMoveSelection.getY();

		switch(keyCode) {
		case KeyEvent.KEYCODE_DPAD_LEFT:
			newMX--;
			break;
		case KeyEvent.KEYCODE_DPAD_RIGHT:
			newMX++;
			break;
		case KeyEvent.KEYCODE_DPAD_UP:
			newMY--;
			break;
		case KeyEvent.KEYCODE_DPAD_DOWN:
			newMY++;
			break;
		}

		boolean bMakeMove = (
				keyCode == KeyEvent.KEYCODE_DPAD_CENTER
				|| keyCode == KeyEvent.KEYCODE_SPACE );

        updateSelection(newMX, newMY, bMakeMove, true);

		return false;
	}


    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if(mIsAnimationRunning) return false;
		
		int action = event.getAction();
		int bx = (int)Math.floor((event.getX() - mBoardRect.left)/mSizeCell);
		int by = (int)Math.floor((event.getY() - mBoardRect.top)/mSizeCell);
		switch (action) {
		case MotionEvent.ACTION_DOWN:
		case MotionEvent.ACTION_MOVE:
			mShowSelectionHelpers = true;
            updateSelection(bx, by, false, true);
            break;
            case MotionEvent.ACTION_UP:
			mShowSelectionHelpers = false;
            updateSelection(bx, by, true, true);
            break;
        }
		return true;
	}

	@Override
	public boolean onTrackballEvent(MotionEvent event) {
		if(mIsAnimationRunning) return false;
		
		float tx = event.getX();
		float ty = event.getY();

		// Log.d("BoardView", String.format("trackball event: %d %f %f", event.getAction(), tx, ty));
		
		switch(event.getAction()) {
		case MotionEvent.ACTION_DOWN: {
            updateSelection(mMoveSelection.getX(), mMoveSelection.getY(), true, true);
        }
        break;

            case MotionEvent.ACTION_MOVE: {
			int newMX = mMoveSelection.getX();
			int newMY = mMoveSelection.getY();
			if( Math.abs(tx)>Math.abs(ty) ) {
		        if( tx>0 ) 
		        	newMX++; 
		        else 
		        	newMX--;
			} else {
		        if( ty>0 ) 
		        	newMY++; 
		        else 
		        	newMY--;
			}
            updateSelection(newMX, newMY, false, true);
        }
        break;

            default:
			return false;
		}

		return true;
	}

    private void updateSelection(int bX, int bY, boolean bMakeMove, boolean bShowSelection) {
        boolean bInvalidate = false;

		if (bX < 0 || bX >= BoardState.boardSize)
			bX = mMoveSelection.getX();

		if (bY < 0 || bY >= BoardState.boardSize)
			bY = mMoveSelection.getY();

        if (mShowSelection != bShowSelection) {
            mShowSelection = bShowSelection;
            bInvalidate = true;
        }

		if( bX!=mMoveSelection.getX() || bY!=mMoveSelection.getY()) {
			mMoveSelection = new Move(bX, bY);
			bInvalidate = true;
		}

			
		if( bMakeMove ) {
			bInvalidate = true;
			mShowSelectionHelpers = false;

			if (getDroidZebra().getState().isValidMove(mMoveSelection)) {
				// if zebra is still thinking - no move is possible yet - throw a busy dialog
				if( mDroidZebra.isThinking() && !mDroidZebra.isHumanToMove()) {
					mDroidZebra.showBusyDialog();
				} else {
		        	try {
						getDroidZebra().makeMove(mMoveSelection);
					} catch (InvalidMove e) {
					} catch (EngineError e) {
						getDroidZebra().FatalError(e.getError());
					}
				}
			}
		}
		
	    if( bInvalidate ) {
			invalidate();
		}
	}

	public void onBoardStateChanged() {
		if( getDroidZebra().mSettingDisplayEnableAnimations ) {
			if( mIsAnimationRunning )
				mAnimationTimer.cancel();
			mIsAnimationRunning = true;
			mAnimationProgress = 0;
			mAnimationTimer.start();
		} else {
			invalidate();
		}
	}
	
}
