#include "inputassembler.h"
#include "rasterizer.h"



static float mv[ 16 ];
static float proj[ 16 ];
static int vertex_format;



/****************************************************************************
 *                         state handling functions                         *
 ****************************************************************************/

void ia_set_modelview_matrix( float* f )
{
    int i;

    for( i=0; i<16; ++i )
        mv[ i ] = f[ i ];
}

void ia_set_projection_matrix( float* f )
{
    int i;

    for( i=0; i<16; ++i )
        proj[ i ] = f[ i ];
}

void ia_set_vertex_format( int format )
{
    vertex_format = format;
}

/****************************************************************************
 *                      triangle processing functions                       *
 ****************************************************************************/

static unsigned char* read_vertex( vertex* v, unsigned char* ptr )
{
    /* initialize vertex structure */
    v->x = 0.0;
    v->y = 0.0;
    v->z = 0.0;
    v->w = 1.0;
    v->r = 1.0;
    v->g = 1.0;
    v->b = 1.0;
    v->a = 1.0;
    v->s[0] = 0.0;
    v->t[0] = 0.0;

    /* decode position */
    if( vertex_format & VF_POSITION_F2 )
    {
        v->x = ((float*)ptr)[0];
        v->y = ((float*)ptr)[1];
        ptr += 2*sizeof(float);
    }
    else if( vertex_format & VF_POSITION_F3 )
    {
        v->x = ((float*)ptr)[0];
        v->y = ((float*)ptr)[1];
        v->z = ((float*)ptr)[2];
        ptr += 3*sizeof(float);
    }
    else if( vertex_format & VF_POSITION_F4 )
    {
        v->x = ((float*)ptr)[0];
        v->y = ((float*)ptr)[1];
        v->z = ((float*)ptr)[2];
        v->w = ((float*)ptr)[3];
        ptr += 4*sizeof(float);
    }

    /* decode color */
    if( vertex_format & VF_COLOR_F3 )
    {
        v->r = ((float*)ptr)[0];
        v->g = ((float*)ptr)[1];
        v->b = ((float*)ptr)[2];
        ptr += 3*sizeof(float);
    }
    else if( vertex_format & VF_COLOR_F4 )
    {
        v->r = ((float*)ptr)[0];
        v->g = ((float*)ptr)[1];
        v->b = ((float*)ptr)[2];
        v->a = ((float*)ptr)[3];
        ptr += 4*sizeof(float);
    }
    else if( vertex_format & VF_COLOR_UB3 )
    {
        v->r = (float)(ptr[0]) / 255.0;
        v->g = (float)(ptr[1]) / 255.0;
        v->b = (float)(ptr[2]) / 255.0;
        ptr += 3;
    }
    else if( vertex_format & VF_COLOR_UB4 )
    {
        v->r = (float)(ptr[0]) / 255.0;
        v->g = (float)(ptr[1]) / 255.0;
        v->b = (float)(ptr[2]) / 255.0;
        v->a = (float)(ptr[3]) / 255.0;
        ptr += 4;
    }

    /* decode texture coordinates */
    if( vertex_format & VF_TEX0 )
    {
        v->s[0] = ((float*)ptr)[0];
        v->t[0] = ((float*)ptr)[1];
        ptr += 2*sizeof(float);
    }

    return ptr;
}

static void transform_vertex( vertex* v )
{
    float x, y, z, w;

    x = mv[0]*v->x + mv[4]*v->y + mv[ 8]*v->z + mv[12]*v->w;
    y = mv[1]*v->x + mv[5]*v->y + mv[ 9]*v->z + mv[13]*v->w;
    z = mv[2]*v->x + mv[6]*v->y + mv[10]*v->z + mv[14]*v->w;
    w = mv[3]*v->x + mv[7]*v->y + mv[11]*v->z + mv[15]*v->w;

    v->x = proj[0]*x + proj[4]*y + proj[ 8]*z + proj[12]*w;
    v->y = proj[1]*x + proj[5]*y + proj[ 9]*z + proj[13]*w;
    v->z = proj[2]*x + proj[6]*y + proj[10]*z + proj[14]*w;
    v->w = proj[3]*x + proj[7]*y + proj[11]*z + proj[15]*w;
}

void ia_draw_triangles( framebuffer* fb, void* ptr, unsigned int vertexcount )
{
    unsigned int i;
    triangle t;

    vertexcount -= vertexcount % 3;

    for( i=0; i<vertexcount; i+=3 )
    {
        ptr = read_vertex( &(t.v0), ptr );
        ptr = read_vertex( &(t.v1), ptr );
        ptr = read_vertex( &(t.v2), ptr );
        transform_vertex( &(t.v0) );
        transform_vertex( &(t.v1) );
        transform_vertex( &(t.v2) );
        triangle_rasterize( &t, fb );
    }
}

void ia_draw_triangles_indexed( framebuffer* fb, void* ptr,
                                unsigned int vertexcount,
                                unsigned short* indices,
                                unsigned int indexcount )
{
    unsigned int vsize = 0;
    unsigned short index;
    unsigned int i;
    triangle t;

    /* determine vertex size in bytes */
         if( vertex_format & VF_POSITION_F2 ) { vsize += 2*sizeof(float); }
    else if( vertex_format & VF_POSITION_F3 ) { vsize += 3*sizeof(float); }
    else if( vertex_format & VF_POSITION_F4 ) { vsize += 4*sizeof(float); }

         if( vertex_format & VF_COLOR_F3  ) { vsize += 3*sizeof(float); }
    else if( vertex_format & VF_COLOR_F4  ) { vsize += 4*sizeof(float); }
    else if( vertex_format & VF_COLOR_UB3 ) { vsize += 3;               }
    else if( vertex_format & VF_COLOR_UB4 ) { vsize += 4;               }

    if( vertex_format & VF_TEX0 )
        vsize += 2*sizeof(float);

    /* for each triangle */
    indexcount -= indexcount % 3;

    for( i=0; i<indexcount; i+=3 )
    {
        /* read first vertex */
        index = indices[ i ];

        if( index>=vertexcount )
            continue;
        
        read_vertex( &(t.v0), ((unsigned char*)ptr) + vsize*index );

        /* read second vertex */
        index = indices[ i+1 ];

        if( index>=vertexcount )
            continue;

        read_vertex( &(t.v1), ((unsigned char*)ptr) + vsize*index );

        /* read third vertex */
        index = indices[ i+2 ];

        if( index>=vertexcount )
            continue;

        read_vertex( &(t.v2), ((unsigned char*)ptr) + vsize*index );

        /* rasterize */
        transform_vertex( &(t.v0) );
        transform_vertex( &(t.v1) );
        transform_vertex( &(t.v2) );
        triangle_rasterize( &t, fb );
    }
}

