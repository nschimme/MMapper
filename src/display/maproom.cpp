/************************************************************************
**
** Authors:   Nils Schimmelmann <nschimme@gmail.com> (Jahara)
**
** This file is part of the MMapper project.
** Maintained by Nils Schimmelmann <nschimme@gmail.com>
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the:
** Free Software Foundation, Inc.
** 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
************************************************************************/

#include "maproom.h"
#include "room.h"

#include <QOpenGLShaderProgram>

struct Vertex {
    GLfloat position[3],
            texcoord[2];
};

MapRoom::MapRoom() :
    m_vertexBuf(QOpenGLBuffer::VertexBuffer),
    m_indexBuf(QOpenGLBuffer::IndexBuffer)
{
    initializeOpenGLFunctions();

    createGeometry();
}

MapRoom::~MapRoom()
{
    m_vertexBuf.destroy();
    m_indexBuf.destroy();
}

void MapRoom::createGeometry()
{
    // we need 4 vertices and 4 colors (1 face, 4 vertices per face)
    // interleaved data -- https://www.opengl.org/wiki/Vertex_Specification#Interleaved_arrays
    Vertex attribs[4] = {
        { { 1.0, 0.0, 0.0}, { 1.0f, 0.0f} },
        { { 1.0, 1.0, 0.0}, { 1.0f, 1.0f} },
        { { 0.0, 1.0, 0.0}, { 0.0f, 1.0f} },
        { { 0.0, 0.0, 0.0}, { 0.0f, 0.0f} },
    };

    // Put all the attribute data in a VBO
    m_vertexBuf.create();
    m_vertexBuf.setUsagePattern( QOpenGLBuffer::StaticDraw );
    m_vertexBuf.bind();
    m_vertexBuf.allocate(attribs, sizeof(attribs));

    // we need 6 indices (1 face, 2 triangles per face, 3 vertices per triangle)
    struct {
        GLubyte square[6];
    } indices;
    for (GLsizei i = 0, v = 0; v < 1 * 4; v += 4) {
        // first triangle (ccw winding)
        indices.square[i++] = v + 0;
        indices.square[i++] = v + 1;
        indices.square[i++] = v + 2;

        // second triangle (ccw winding)
        indices.square[i++] = v + 0;
        indices.square[i++] = v + 2;
        indices.square[i++] = v + 3;
    }

    // Put all the index data in a IBO
    m_indexBuf.create();
    m_indexBuf.setUsagePattern( QOpenGLBuffer::StaticDraw );
    m_indexBuf.bind();
    m_indexBuf.allocate(&indices, sizeof(indices));
}

void MapRoom::draw(QOpenGLShaderProgram *program)
{
    // Tell OpenGL which VBOs to use
    m_vertexBuf.bind();
    m_indexBuf.bind();

    // Configure the vertex streams for this attribute data layout
    int vertexLocation = program->attributeLocation("position");
    program->enableAttributeArray(vertexLocation);
    program->setAttributeBuffer(vertexLocation, GL_FLOAT, offsetof(Vertex, position), 3,
                                sizeof(Vertex) );
    int texcoordLocation = program->attributeLocation("texCoord2d");
    program->enableAttributeArray(texcoordLocation);
    program->setAttributeBuffer(texcoordLocation, GL_FLOAT, offsetof(Vertex, texcoord), 3,
                                sizeof(Vertex) );

    // Draw
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);

    program->disableAttributeArray(vertexLocation);
    program->disableAttributeArray(texcoordLocation);

    m_vertexBuf.release();
    m_indexBuf.release();
}

