/*
    Copyright 2007 Dmitry Suzdalev <dimsuz@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "minefielditem.h"

#include <kdebug.h>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

#include "cellitem.h"
#include "renderer.h"

MineFieldItem::MineFieldItem( int numRows, int numCols, int numMines )
{
    regenerateField(numRows, numCols, numMines);
}

void MineFieldItem::regenerateField( int numRows, int numCols, int numMines )
{
    Q_ASSERT( numMines < numRows*numRows );

    int oldSize = m_cells.size();
    int newSize = numRows*numCols;

    // if field is being shrinked, delete elements at the end before resizing vector
    if(oldSize > newSize)
    {
        for( int i=newSize; i<oldSize; ++i )
        {
            // is this the best way to remove an item?
            scene()->removeItem(m_cells[i]);
            delete m_cells[i];
        }
    }

    m_cells.resize(newSize);

    m_numRows = numRows;
    m_numCols = numCols;
    m_minesCount = numMines;

    for(int i=0; i<newSize; ++i)
    {
        // reset old, create new
        if(i<oldSize)
            m_cells[i]->reset();
        else
            m_cells[i] = new CellItem(this);
    }

    // generating mines
    int minesToPlace = m_minesCount;
    int randomIdx = 0;
    while(minesToPlace != 0)
    {
        randomIdx = m_randomSeq.getLong( m_numRows*m_numCols );
        if(!m_cells.at(randomIdx)->hasMine())
        {
            m_cells.at(randomIdx)->setHasMine(true);
            minesToPlace--;
        }
        else
            continue;
    }

    // calculating digits for cells around mines
    for(int row=0; row < m_numRows; ++row)
        for(int col=0; col < m_numCols; ++col)
        {
            if(itemAt(row,col)->hasMine())
                continue;
            // simply looking at all 8 neighbour cells and adding +1 for each
            // mine we found
            int resultingDigit = 0;
            if(row != 0 && col != 0) // upper-left diagonal
            {
                if(itemAt(row-1,col-1)->hasMine())
                    resultingDigit++;
            }
            if(row != 0) // upper
            {
                if(itemAt(row-1, col)->hasMine())
                    resultingDigit++;
            }
            if(row != 0 && col != m_numCols-1) // upper-right diagonal
            {
                if(itemAt(row-1, col+1)->hasMine())
                    resultingDigit++;
            }
            if(col != 0) // on the left
            {
                if(itemAt(row,col-1)->hasMine())
                    resultingDigit++;
            }
            if(col != m_numCols-1) // on the right
            {
                if(itemAt(row, col+1)->hasMine())
                    resultingDigit++;
            }
            if(row != m_numRows-1 && col != 0) // bottom-left diagonal
            {
                if(itemAt(row+1, col-1)->hasMine())
                    resultingDigit++;
            }
            if(row != m_numRows-1) // bottom
            {
                if(itemAt(row+1, col)->hasMine())
                    resultingDigit++;
            }
            if(row != m_numRows-1 && col != m_numCols-1) // bottom-right diagonal
            {
                if(itemAt(row+1, col+1)->hasMine())
                    resultingDigit++;
            }
            // having 0 is ok here - it'll be empty
            itemAt(row,col)->setDigit(resultingDigit);
        }

    adjustItemPositions();
    m_flaggedMinesCount = 0;
    emit flaggedMinesCountChanged(m_flaggedMinesCount);
}

QRectF MineFieldItem::boundingRect() const
{
    // we assume that all items have the same size
    // so let's take the first item's size
    qreal cellSize = KMinesRenderer::self()->cellSize();
    return QRectF(0, 0, cellSize*m_numCols, cellSize*m_numRows);
}

void MineFieldItem::paint( QPainter * painter, const QStyleOptionGraphicsItem* opt, QWidget* w)
{
    Q_UNUSED(painter);
    Q_UNUSED(opt);
    Q_UNUSED(w);
}

void MineFieldItem::resizeToFitInRect(const QRectF& rect)
{
    prepareGeometryChange();

    // here follows "cooomplex" algorithm to choose which side to
    // take when calculating cell size by dividing this side by
    // numRows or numCols correspondingly
    // it's cooomplex, because I have to paint some figures on paper
    // to understand that criteria for choosing one side or another (for
    // determining cell size from it) is comparing
    // cols/r.width() and rows/r.height():
    bool chooseHorizontalSide = m_numCols / rect.width() > m_numRows / rect.height();

    qreal size = 0;
    if( chooseHorizontalSide )
        size = rect.width() / m_numCols;
    else
        size = rect.height() / m_numRows;

    KMinesRenderer::self()->setCellSize( static_cast<int>(size) );

    foreach( CellItem* item, m_cells )
        item->updatePixmap();

    adjustItemPositions();
}

void MineFieldItem::adjustItemPositions()
{
    Q_ASSERT( m_cells.size() == m_numRows*m_numCols );

    int itemSize = KMinesRenderer::self()->cellSize();

    for(int row=0; row<m_numRows; ++row)
        for(int col=0; col<m_numCols; ++col)
        {
            itemAt(row,col)->setPos(col*itemSize, row*itemSize);
        }
}

void MineFieldItem::onItemRevealed(int row, int col)
{
    if(itemAt(row,col)->hasMine())
    {
        // reveal all field
        foreach( CellItem* item, m_cells )
        {
            if( (item->isFlagged() && !item->hasMine()) || (!item->isFlagged() && item->hasMine()) )
                item->reveal();
        }
    }
    else if(itemAt(row,col)->digit() == 0) // empty cell
    {
        // this item is already revealed, but let's unreveal it
        // just to keep the revealEmptySpace() look more clean
        // (without checks for isRevealed() in every if there, but
        // with one check at the beginning
        itemAt(row,col)->unreveal();
        revealEmptySpace(row,col);
    }
}

void MineFieldItem::revealEmptySpace(int row, int col)
{
    if(itemAt(row,col)->isRevealed() || itemAt(row,col)->isFlagged())
        return;

    itemAt(row,col)->reveal();

    CellItem *item = 0;
    // recursively reveal neighbour cells until we find cells with digit
    if(row != 0 && col != 0) // upper-left diagonal
    {
        item = itemAt(row-1,col-1);
        if(item->digit() == 0)
            revealEmptySpace(row-1,col-1);
        else if(!item->isFlagged())
            item->reveal();
    }
    if(row != 0) // upper
    {
        item = itemAt(row-1,col);
        if(item->digit() == 0)
            revealEmptySpace(row-1,col);
        else if(!item->isFlagged())
            item->reveal();
    }
    if(row != 0 && col != m_numCols-1) // upper-right diagonal
    {
        item = itemAt(row-1,col+1);
        if(item->digit() == 0)
            revealEmptySpace(row-1,col+1);
        else if(!item->isFlagged())
            item->reveal();
    }
    if(col != 0) // on the left
    {
        item = itemAt(row,col-1);
        if(item->digit() == 0)
            revealEmptySpace(row,col-1);
        else if(!item->isFlagged())
            item->reveal();
    }
    if(col != m_numCols-1) // on the right
    {
        item = itemAt(row,col+1);
        if(item->digit() == 0)
            revealEmptySpace(row,col+1);
        else if(!item->isFlagged())
            item->reveal();
    }
    if(row != m_numRows-1 && col != 0) // bottom-left diagonal
    {
        item = itemAt(row+1,col-1);
        if(item->digit() == 0)
            revealEmptySpace(row+1,col-1);
        else if(!item->isFlagged())
            item->reveal();
    }
    if(row != m_numRows-1) // bottom
    {
        item = itemAt(row+1,col);
        if(item->digit() == 0)
            revealEmptySpace(row+1,col);
        else if(!item->isFlagged())
            item->reveal();
    }
    if(row != m_numRows-1 && col != m_numCols-1) // bottom-right diagonal
    {
        item = itemAt(row+1, col+1);
        if(item->digit() == 0)
            revealEmptySpace(row+1,col+1);
        else if(!item->isFlagged())
            item->reveal();
    }
}

void MineFieldItem::mousePressEvent( QGraphicsSceneMouseEvent *ev )
{
    if(ev->button() == Qt::LeftButton)
    {
        int itemSize = KMinesRenderer::self()->cellSize();

        int row = static_cast<int>(ev->pos().y()/itemSize);
        int col = static_cast<int>(ev->pos().x()/itemSize);
        itemAt(row,col)->press();
        m_rowcolMousePress = qMakePair(row,col);
    }
}

void MineFieldItem::mouseReleaseEvent( QGraphicsSceneMouseEvent * ev)
{
    int itemSize = KMinesRenderer::self()->cellSize();

    int row = static_cast<int>(ev->pos().y()/itemSize);
    int col = static_cast<int>(ev->pos().x()/itemSize);
    CellItem* itemUnderMouse = itemAt(row,col);

    if(ev->button() == Qt::LeftButton)
    {
        CellItem *item = itemAt(m_rowcolMousePress.first, m_rowcolMousePress.second);
        item->release();
        if(item->isRevealed())
            onItemRevealed(m_rowcolMousePress.first,m_rowcolMousePress.second);
    }
    else if(ev->button() == Qt::RightButton)
    {
        bool wasFlagged = itemUnderMouse->isFlagged();
        itemUnderMouse->mark();
        bool flagStateChanged = (itemUnderMouse->isFlagged() != wasFlagged);
        if(flagStateChanged)
        {
            if(itemUnderMouse->isFlagged())
                m_flaggedMinesCount++;
            else
                m_flaggedMinesCount--;
            emit flaggedMinesCountChanged(m_flaggedMinesCount);
        }
    }
}
