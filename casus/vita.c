/* vita.c — ludus vitae Conway */
#include <stdio.h>
#include <string.h>

#define LAT 10
#define ALT 10

int main( void )
{
    int tab[ ALT ][ LAT ] ;
    int nov[ ALT ][ LAT ] ;

    memset( tab, 0, sizeof( tab ) ) ;

    /* volucris */
    tab[ 1 ][ 2 ] = 1 ;
    tab[ 2 ][ 3 ] = 1 ;
    tab[ 3 ][ 1 ] = 1 ;
    tab[ 3 ][ 2 ] = 1 ;
    tab[ 3 ][ 3 ] = 1 ;

    for ( int gen = 0 ; gen < 5 ; gen++ )
    {
        printf( "generatio %d:\n", gen ) ;
        for ( int y = 0 ; y < ALT ; y++ )
        {
            for ( int x = 0 ; x < LAT ; x++ )
                printf( "%c", tab[ y ][ x ] ? '#' : '.' ) ;
            printf( "\n" ) ;
        }
        printf( "\n" ) ;

        memset( nov, 0, sizeof( nov ) ) ;
        for ( int y = 0 ; y < ALT ; y++ )
            for ( int x = 0 ; x < LAT ; x++ )
            {
                int vic = 0 ;
                for ( int dy = -1 ; dy <= 1 ; dy++ )
                    for ( int dx = -1 ; dx <= 1 ; dx++ )
                    {
                        if ( dx == 0 && dy == 0 ) continue ;
                        int ny = y + dy ;
                        int nx = x + dx ;
                        if ( ny >= 0 && ny < ALT && nx >= 0 && nx < LAT )
                            vic += tab[ ny ][ nx ] ;
                    }
                if ( tab[ y ][ x ] )
                    nov[ y ][ x ] = ( vic == 2 || vic == 3 ) ;
                else
                    nov[ y ][ x ] = ( vic == 3 ) ;
            }
        memcpy( tab, nov, sizeof( tab ) ) ;
    }
    return 0 ;
}
