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
    : m_rowcolMousePress(-1,-1), m_rowcolMidButton(-1,-1), m_gameOver(false)
{
    regenerateField(numRows, numCols, numMines);
}

void MineFieldItem::regenerateField( int numRows, int numCols, int numMines )
{
    Q_ASSERT( numMines < numRows*numRows );

    m_gameOver = false;

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
    m_numUnrevealed = m_numRows*m_numCols;

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
            QList<CellItem*> neighbours = adjasentItemsFor(row,col);
            foreach(CellItem* item, neighbours)
            {
                if(item->hasMine())
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
    m_numUnrevealed--;
    if(itemAt(row,col)->hasMine())
    {
        revealAllMines();
    }
    else if(itemAt(row,col)->digit() == 0) // empty cell
    {
        revealEmptySpace(row,col);
    }
    kDebug() << "Num unrevealed: " << m_numUnrevealed << endl;
    // now let's check for possible win/loss
    checkLost();
    if(!m_gameOver) // checkLost might set it
        checkWon();
}

void MineFieldItem::revealEmptySpace(int row, int col)
{
    kDebug() << "revealEmptySpace" << row << "," << col << endl;

    // recursively reveal neighbour cells until we find cells with digit
    typedef QPair<int,int> RowColPair;
    QList<RowColPair> list = adjasentRowColsFor(row,col);
    CellItem *item = 0;

    foreach( const RowColPair& pair, list )
    {
        // first is row, second is col
        item = itemAt(pair.first,pair.second);
        if(item->isRevealed() || item->isFlagged())
            continue;
        if(item->digit() == 0)
        {
            item->reveal();
            m_numUnrevealed--;
            revealEmptySpace(pair.first,pair.second);
        }
        else if(!item->isFlagged())
        {
            item->reveal();
            m_numUnrevealed--;
        }
    }
}

void MineFieldItem::mousePressEvent( QGraphicsSceneMouseEvent *ev )
{
    int itemSize = KMinesRenderer::self()->cellSize();

    int row = static_cast<int>(ev->pos().y()/itemSize);
    int col = static_cast<int>(ev->pos().x()/itemSize);
    if(ev->button() == Qt::LeftButton)
    {
        if(m_rowcolMidButton.first != -1) // mid-button is already pressed
            return;

        itemAt(row,col)->press();
        // TODO: rename rowcolLeftButton
        m_rowcolMousePress = qMakePair(row,col);
    }
    else if(ev->button() == Qt::MidButton)
    {
        QList<CellItem*> neighbours = adjasentItemsFor(row,col);
        foreach(CellItem* item, neighbours)
        {
            if(!item->isFlagged() && !item->isRevealed())
                item->press();
            m_rowcolMidButton = qMakePair(row,col);
        }
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
        if(m_rowcolMidButton.first != -1) // mid-button is already pressed
            return;

        CellItem *item = itemAt(m_rowcolMousePress.first, m_rowcolMousePress.second);

        if(!item->isRevealed()) // revealing only unrevealed ones
        {
            item->release();
            if(item->isRevealed())
                onItemRevealed(m_rowcolMousePress.first,m_rowcolMousePress.second);
        }
        m_rowcolMousePress = qMakePair(-1,-1);//reset
    }
    else if(ev->button() == Qt::RightButton)
    {
        if(m_rowcolMidButton.first != -1) // mid-button is already pressed
            return;

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
        m_rowcolMousePress = qMakePair(-1,-1);//reset
    }
    else if( ev->button() == Qt::MidButton )
    {
        m_rowcolMidButton = qMakePair(-1,-1);

        QList<CellItem*> neighbours = adjasentItemsFor(row,col);
        if(!itemUnderMouse->isRevealed())
        {
            foreach(CellItem *item, neighbours)
                item->undoPress();
            return;
        }

        int numFlags = 0;
        int numMines = 0;
        foreach(CellItem *item, neighbours)
        {
            if(item->isFlagged())
                numFlags++;
            if(item->hasMine())
                numMines++;
        }
        if(numFlags == numMines && numFlags != 0)
        {
            foreach(CellItem *item, neighbours)
            {
                if(!item->isRevealed()) // revealing only unrevealed ones
                {
                    // force=true to omit Pressed check
                    item->release(true);
                    if(item->isRevealed())
                        onItemRevealed(item);
                }
            }
        }
        else
        {
            foreach(CellItem *item, neighbours)
                item->undoPress();
        }
    }
}

void MineFieldItem::mouseMoveEvent( QGraphicsSceneMouseEvent *ev )
{
    int itemSize = KMinesRenderer::self()->cellSize();

    int row = static_cast<int>(ev->pos().y()/itemSize);
    int col = static_cast<int>(ev->pos().x()/itemSize);

    if(ev->buttons() & Qt::MidButton)
    {
        if(m_rowcolMidButton.first != row || m_rowcolMidButton.second != col)
        {
            // un-press previously pressed cells
            QList<CellItem*> prevNeighbours = adjasentItemsFor(m_rowcolMidButton.first,
                                                              m_rowcolMidButton.second);
            foreach(CellItem *item, prevNeighbours)
                   item->undoPress();

            // and press current neighbours
            QList<CellItem*> neighbours = adjasentItemsFor(row,col);
            foreach(CellItem *item, neighbours)
                item->press();

            m_rowcolMidButton = qMakePair(row,col);
        }
    }
}

void MineFieldItem::revealAllMines()
{
    foreach( CellItem* item, m_cells )
    {
        if( (item->isFlagged() && !item->hasMine()) || (!item->isFlagged() && item->hasMine()) )
        {
            item->reveal();
            m_numUnrevealed--;
        }
    }
}

void MineFieldItem::onItemRevealed(CellItem* item)
{
    int idx = m_cells.indexOf(item);
    if(idx == -1)
    {
        kDebug() << "really strange - item not found" << endl;
        return;
    }

    int row = idx / m_numCols;
    int col = idx - row*m_numCols;
    onItemRevealed(row,col);
}

void MineFieldItem::checkLost()
{
    // for loss...
    foreach( CellItem* item, m_cells )
    {
        if(item->isExploded())
        {
            m_gameOver = true;
            emit gameOver(false);
            break;
        }
    }
}

void MineFieldItem::checkWon()
{
    // let's check the trivial case when
    // only some cells left unflagged and they
    // all contain bombs. this counts as win
    if(m_numUnrevealed == m_minesCount)
            emit gameOver(true);
}

QList<QPair<int,int> > MineFieldItem::adjasentRowColsFor(int row, int col)
{
    QList<QPair<int,int> > resultingList;
    if(row != 0 && col != 0) // upper-left diagonal
        resultingList.append( qMakePair(row-1,col-1) );
    if(row != 0) // upper
        resultingList.append(qMakePair(row-1, col));
    if(row != 0 && col != m_numCols-1) // upper-right diagonal
        resultingList.append(qMakePair(row-1, col+1));
    if(col != 0) // on the left
        resultingList.append(qMakePair(row,col-1));
    if(col != m_numCols-1) // on the right
        resultingList.append(qMakePair(row, col+1));
    if(row != m_numRows-1 && col != 0) // bottom-left diagonal
        resultingList.append(qMakePair(row+1, col-1));
    if(row != m_numRows-1) // bottom
        resultingList.append(qMakePair(row+1, col));
    if(row != m_numRows-1 && col != m_numCols-1) // bottom-right diagonal
        resultingList.append(qMakePair(row+1, col+1));
    return resultingList;
}

QList<CellItem*> MineFieldItem::adjasentItemsFor(int row, int col)
{
    QList<QPair<int,int> > rowcolList = adjasentRowColsFor(row,col);
    QList<CellItem*> resultingList;
    typedef QPair<int,int> RowColPair;
    foreach( const RowColPair& pair, rowcolList )
        resultingList.append( itemAt(pair.first, pair.second) );
    return resultingList;
}
