#include <stdio.h>
#include <AR/ar.h>
#include <connection/ConnectionManager.h>
#include <android/log.h>

static ARMarkerInfo2          *marker_info2;
static ARMarkerInfo           *wmarker_info;
static int                    wmarker_num = 0;

static arPrevInfo             prev_info[AR_SQUARE_MAX];
static int                    prev_num = 0;

static arPrevInfo             sprev_info[2][AR_SQUARE_MAX];
static int                    sprev_num[2] = {0,0};


JavaVM *g_jvm = NULL;
static jobject g_obj;
static jmethodID g_callback;
static jclass cls;


JNIEXPORT void JNICALL Java_proxy_ConnectionManager_register(JNIEnv * env, jobject obj) {
//	(*env)-> MonitorEnter(env,obj);
	__android_log_print(ANDROID_LOG_INFO,"JNI2","Java_proxy_ConnectionManager_register");
      cls = (*env)->GetObjectClass(env, obj);
 //     cls = (*env)->NewGlobalRef(env,extractedCls);
      __android_log_print(ANDROID_LOG_INFO,"JNI2","cls attained");
      (*env)->GetJavaVM(env, &g_jvm);
      __android_log_print(ANDROID_LOG_INFO,"JNI2","env attained");
      g_obj = obj;
      __android_log_print(ANDROID_LOG_INFO,"JNI2","setting g_callback");
      g_callback = (*env)->GetMethodID(env, cls, "updatePawn", "(IFF)V");
      __android_log_print(ANDROID_LOG_INFO,"JNI2","g_callback set");
//      (*env)-> MonitorExit(env,obj);
}


int arSavePatt( ARUint8 *image, ARMarkerInfo *marker_info, char *filename )
{
    FILE      *fp;
    ARUint8   ext_pat[4][AR_PATT_SIZE_Y][AR_PATT_SIZE_X][3];
    int       vertex[4];
    int       i, j, k, x, y;

	// Match supplied info against previously recognised marker.
    for( i = 0; i < wmarker_num; i++ ) {
        if( marker_info->area   == marker_info2[i].area
         && marker_info->pos[0] == marker_info2[i].pos[0]
         && marker_info->pos[1] == marker_info2[i].pos[1] ) break;
    }
    if( i == wmarker_num ) return -1;

    for( j = 0; j < 4; j++ ) {
        for( k = 0; k < 4; k++ ) {
            vertex[k] = marker_info2[i].vertex[(k+j+2)%4];
        }
        arGetPatt( image, marker_info2[i].x_coord,
                   marker_info2[i].y_coord, vertex, ext_pat[j] );
    }

    fp = fopen( filename, "w" );
    if( fp == NULL ) return -1;

	// Write out in order AR_PATT_SIZE_X columns x AR_PATT_SIZE_Y rows x 3 colours x 4 orientations.
    for( i = 0; i < 4; i++ ) {
        for( j = 0; j < 3; j++ ) {
            for( y = 0; y < AR_PATT_SIZE_Y; y++ ) {
                for( x = 0; x < AR_PATT_SIZE_X; x++ ) {
                    fprintf( fp, "%4d", ext_pat[i][y][x][j] );
                }
                fprintf(fp, "\n");
            }
        }
        fprintf(fp, "\n");
    }

    fclose( fp );

    return 0;
}

int arDetectMarker( ARUint8 *dataPtr, int thresh,
                    ARMarkerInfo **marker_info, int *marker_num )
{
    ARInt16                *limage;
    int                    label_num;
    int                    *area, *clip, *label_ref;
    double                 *pos;
    double                 rarea, rlen, rlenmin;
    double                 diff, diffmin;
    int                    cid, cdir;
    int                    i, j, k;
#ifdef DEBUG_LOGGING
        __android_log_write(ANDROID_LOG_INFO,"AR","entered arDetectMarker");
#endif

    *marker_num = 0;

    limage = arLabeling( dataPtr, thresh,
                         &label_num, &area, &pos, &clip, &label_ref );
    if( limage == 0 ) {
#ifdef DEBUG_LOGGING
        __android_log_write(ANDROID_LOG_INFO,"AR","arLabeling not successful(returned 0)");
#endif
        return -1;
    }
#ifdef DEBUG_LOGGING
   __android_log_print(ANDROID_LOG_INFO,"AR","detected %d labels",label_num);
#endif

    marker_info2 = arDetectMarker2( limage, label_num, label_ref,
                                    area, pos, clip, AR_AREA_MAX, AR_AREA_MIN,
                                    1.0, &wmarker_num);
    if( marker_info2 == 0 ) {
#ifdef DEBUG_LOGGING
        __android_log_write(ANDROID_LOG_INFO,"AR","arDetectMarker not successful(returned 0)");
#endif
	return -1;
    }
#ifdef DEBUG_LOGGING
   __android_log_print(ANDROID_LOG_INFO,"AR","got %d marker candidates",wmarker_num);
#endif


    wmarker_info = arGetMarkerInfo( dataPtr, marker_info2, &wmarker_num );
    if( wmarker_info == 0 ) {
#ifdef DEBUG_LOGGING
        __android_log_write(ANDROID_LOG_INFO,"AR","arGetMarkerInfo not successful(returned 0)");
#endif
        return -1;
    }
#ifdef DEBUG_LOGGING
   __android_log_print(ANDROID_LOG_INFO,"AR","got marker_info for %d markers",wmarker_num);
#endif


    for( i = 0; i < prev_num; i++ ) {
        rlenmin = 10.0;
        cid = -1;
        for( j = 0; j < wmarker_num; j++ ) {
            rarea = (double)prev_info[i].marker.area / (double)wmarker_info[j].area;
            if( rarea < 0.7 || rarea > 1.43 ) {
#ifdef DEBUG_LOGGING
           __android_log_print(ANDROID_LOG_INFO,"AR","relative area of marker %d too low or too high (%f)",i,rarea);
#endif
	    	continue;
	    }
            rlen = ( (wmarker_info[j].pos[0] - prev_info[i].marker.pos[0])
                   * (wmarker_info[j].pos[0] - prev_info[i].marker.pos[0])
                   + (wmarker_info[j].pos[1] - prev_info[i].marker.pos[1])
                   * (wmarker_info[j].pos[1] - prev_info[i].marker.pos[1]) ) / wmarker_info[j].area;
            if( rlen < 0.5 && rlen < rlenmin ) {
                rlenmin = rlen;
                cid = j;
            }
        }
        if( cid >= 0 && wmarker_info[cid].cf < prev_info[i].marker.cf ) {
            wmarker_info[cid].cf = prev_info[i].marker.cf;
            wmarker_info[cid].id = prev_info[i].marker.id;
            diffmin = 10000.0 * 10000.0;
            cdir = -1;
            for( j = 0; j < 4; j++ ) {
                diff = 0;
                for( k = 0; k < 4; k++ ) {
                    diff += (prev_info[i].marker.vertex[k][0] - wmarker_info[cid].vertex[(j+k)%4][0])
                          * (prev_info[i].marker.vertex[k][0] - wmarker_info[cid].vertex[(j+k)%4][0])
                          + (prev_info[i].marker.vertex[k][1] - wmarker_info[cid].vertex[(j+k)%4][1])
                          * (prev_info[i].marker.vertex[k][1] - wmarker_info[cid].vertex[(j+k)%4][1]);
                }
                if( diff < diffmin ) {
                    diffmin = diff;
                    cdir = (prev_info[i].marker.dir - j + 4) % 4;
                }
            }
            wmarker_info[cid].dir = cdir;
        }
    }

    for( i = 0; i < wmarker_num; i++ ) {
/*
	printf("cf = %g\n", wmarker_info[i].cf);
*/
#ifdef DEBUG_LOGGING
    		int pawnId = i;
    		float x = wmarker_info[i].pos[1]/320.0;
    		float y = wmarker_info[i].pos[0]/480.0;
           __android_log_print(ANDROID_LOG_INFO,"AR","cf[%d] = %g id[%d]=%d",i,wmarker_info[i].cf,i,wmarker_info[i].id);
           __android_log_print(ANDROID_LOG_INFO,"TOM","============================================================");
           __android_log_print(ANDROID_LOG_INFO,"TOM","wmarker_info[%d].POSITION: Y = %f; X = %f",i,wmarker_info[i].pos[0], wmarker_info[i].pos[1]);
           __android_log_print(ANDROID_LOG_INFO,"TOM","wmarker_info[%d].RELATIVE_POSITION: Y = %f; X = %f",i,wmarker_info[i].pos[0]/480.0, wmarker_info[i].pos[1]/320.0);
           __android_log_print(ANDROID_LOG_INFO,"TOM","wmarker_info[%d].vertex[0][0] = %f",i,wmarker_info[i].vertex[0][0]);
           __android_log_print(ANDROID_LOG_INFO,"TOM","wmarker_info[%d].vertex[0][1] = %f",i,wmarker_info[i].vertex[0][1]);
           __android_log_print(ANDROID_LOG_INFO,"TOM","wmarker_info[%d].vertex[1][0] = %f",i,wmarker_info[i].vertex[1][0]);
           __android_log_print(ANDROID_LOG_INFO,"TOM","wmarker_info[%d].vertex[1][1] = %f",i,wmarker_info[i].vertex[1][1]);
           __android_log_print(ANDROID_LOG_INFO,"TOM","wmarker_info[%d].vertex[2][0] = %f",i,wmarker_info[i].vertex[2][0]);
           __android_log_print(ANDROID_LOG_INFO,"TOM","wmarker_info[%d].vertex[2][1] = %f",i,wmarker_info[i].vertex[2][1]);
           __android_log_print(ANDROID_LOG_INFO,"TOM","wmarker_info[%d].vertex[3][0] = %f",i,wmarker_info[i].vertex[3][0]);
           __android_log_print(ANDROID_LOG_INFO,"TOM","wmarker_info[%d].vertex[3][1] = %f",i,wmarker_info[i].vertex[3][1]);
           jint jPawnId = (jint) pawnId;
           jfloat jx = (jfloat) x;
           jfloat jy = (jfloat) y;
           if (g_jvm != NULL) {

        	   __android_log_print(ANDROID_LOG_INFO,"JNI","JVM environment not null");
               JNIEnv *env;
               (*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_4);
   //            (*env)-> MonitorEnter(env,g_obj);

               __android_log_print(ANDROID_LOG_INFO,"JNI","got environment");
               __android_log_print(ANDROID_LOG_INFO,"JNI","environment == null: %s",(env == NULL)?"true":"false");
               __android_log_print(ANDROID_LOG_INFO,"JNI","g_obj == null: %s",(g_obj == NULL)?"true":"false");
               __android_log_print(ANDROID_LOG_INFO,"JNI","g_callback == null: %s",(g_callback == NULL)?"true":"false");
               __android_log_print(ANDROID_LOG_INFO,"JNI","calling method");
               __android_log_print(ANDROID_LOG_INFO,"JNI","pawnId = %d x = %f y = %f", pawnId, x, y);


               (*env)->CallVoidMethod(env, g_obj, g_callback, jPawnId, jx, jy);

               __android_log_print(ANDROID_LOG_INFO,"JNI","method called");
               /* pawnId must be int, x, y must be float of course */
//               (*env)-> MonitorExit(env,g_obj);
           }
#endif
        if( wmarker_info[i].cf < 0.5 ) { 
#ifdef DEBUG_LOGGING
           __android_log_print(ANDROID_LOG_INFO,"AR","confidence value of marker %d too low(%f < 0.5)",i,wmarker_info[i].cf);
#endif
		wmarker_info[i].id = -1;
	}
   }


/*------------------------------------------------------------*/

    for( i = j = 0; i < prev_num; i++ ) {
        prev_info[i].count++;
        if( prev_info[i].count < 4 ) {
            prev_info[j] = prev_info[i];
            j++;
        }
    }
    prev_num = j;

    for( i = 0; i < wmarker_num; i++ ) {
        if( wmarker_info[i].id < 0 ) continue;

        for( j = 0; j < prev_num; j++ ) {
            if( prev_info[j].marker.id == wmarker_info[i].id ) break;
        }
        prev_info[j].marker = wmarker_info[i];
        prev_info[j].count  = 1;
        if( j == prev_num ) prev_num++;
    }

    for( i = 0; i < prev_num; i++ ) {
        for( j = 0; j < wmarker_num; j++ ) {
            rarea = (double)prev_info[i].marker.area / (double)wmarker_info[j].area;
            if( rarea < 0.7 || rarea > 1.43 ) continue;
            rlen = ( (wmarker_info[j].pos[0] - prev_info[i].marker.pos[0])
                   * (wmarker_info[j].pos[0] - prev_info[i].marker.pos[0])
                   + (wmarker_info[j].pos[1] - prev_info[i].marker.pos[1])
                   * (wmarker_info[j].pos[1] - prev_info[i].marker.pos[1]) ) / wmarker_info[j].area;
            if( rlen < 0.5 ) break;
        }
        if( j == wmarker_num ) {
            wmarker_info[wmarker_num] = prev_info[i].marker;
            wmarker_num++;
        }
    }


    *marker_num  = wmarker_num;
    *marker_info = wmarker_info;

#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_INFO,"AR","left arDetectMarker, detected %d markers", wmarker_num);
#endif

    return 0;
}


int arDetectMarkerLite( ARUint8 *dataPtr, int thresh,
                        ARMarkerInfo **marker_info, int *marker_num )
{
    ARInt16                *limage;
    int                    label_num;
    int                    *area, *clip, *label_ref;
    double                 *pos;
    int                    i;

    *marker_num = 0;

    limage = arLabeling( dataPtr, thresh,
                         &label_num, &area, &pos, &clip, &label_ref );
    if( limage == 0 )    return -1;

    marker_info2 = arDetectMarker2( limage, label_num, label_ref,
                                    area, pos, clip, AR_AREA_MAX, AR_AREA_MIN,
                                    1.0, &wmarker_num);
    if( marker_info2 == 0 ) return -1;

    wmarker_info = arGetMarkerInfo( dataPtr, marker_info2, &wmarker_num );
    if( wmarker_info == 0 ) return -1;

    for( i = 0; i < wmarker_num; i++ ) {
        if( wmarker_info[i].cf < 0.5 ) wmarker_info[i].id = -1;
    }


    *marker_num  = wmarker_num;
    *marker_info = wmarker_info;

    return 0;
}

int arsDetectMarker( ARUint8 *dataPtr, int thresh,
                     ARMarkerInfo **marker_info, int *marker_num, int LorR )
{
    ARInt16                *limage;
    int                    label_num;
    int                    *area, *clip, *label_ref;
    double                 *pos;
    double                 rarea, rlen, rlenmin;
    double                 diff, diffmin;
    int                    cid, cdir;
    int                    i, j, k;

    *marker_num = 0;

    limage = arsLabeling( dataPtr, thresh,
                          &label_num, &area, &pos, &clip, &label_ref, LorR );
    if( limage == 0 )    return -1;

    marker_info2 = arDetectMarker2( limage, label_num, label_ref,
                                    area, pos, clip, AR_AREA_MAX, AR_AREA_MIN,
                                    1.0, &wmarker_num);
    if( marker_info2 == 0 ) return -1;

    wmarker_info = arsGetMarkerInfo( dataPtr, marker_info2, &wmarker_num, LorR );
    if( wmarker_info == 0 ) return -1;

    for( i = 0; i < sprev_num[LorR]; i++ ) {
        rlenmin = 10.0;
        cid = -1;
        for( j = 0; j < wmarker_num; j++ ) {
            rarea = (double)sprev_info[LorR][i].marker.area / (double)wmarker_info[j].area;
            if( rarea < 0.7 || rarea > 1.43 ) continue;
            rlen = ( (wmarker_info[j].pos[0] - sprev_info[LorR][i].marker.pos[0])
                   * (wmarker_info[j].pos[0] - sprev_info[LorR][i].marker.pos[0])
                   + (wmarker_info[j].pos[1] - sprev_info[LorR][i].marker.pos[1])
                   * (wmarker_info[j].pos[1] - sprev_info[LorR][i].marker.pos[1]) ) / wmarker_info[j].area;
            if( rlen < 0.5 && rlen < rlenmin ) {
                rlenmin = rlen;
                cid = j;
            }
        }
        if( cid >= 0 && wmarker_info[cid].cf < sprev_info[LorR][i].marker.cf ) {
            wmarker_info[cid].cf = sprev_info[LorR][i].marker.cf;
            wmarker_info[cid].id = sprev_info[LorR][i].marker.id;
            diffmin = 10000.0 * 10000.0;
            cdir = -1;
            for( j = 0; j < 4; j++ ) {
                diff = 0;
                for( k = 0; k < 4; k++ ) {
                    diff += (sprev_info[LorR][i].marker.vertex[k][0] - wmarker_info[cid].vertex[(j+k)%4][0])
                          * (sprev_info[LorR][i].marker.vertex[k][0] - wmarker_info[cid].vertex[(j+k)%4][0])
                          + (sprev_info[LorR][i].marker.vertex[k][1] - wmarker_info[cid].vertex[(j+k)%4][1])
                          * (sprev_info[LorR][i].marker.vertex[k][1] - wmarker_info[cid].vertex[(j+k)%4][1]);
                }
                if( diff < diffmin ) {
                    diffmin = diff;
                    cdir = (sprev_info[LorR][i].marker.dir - j + 4) % 4;
                }
            }
            wmarker_info[cid].dir = cdir;
        }
    }

    for( i = 0; i < wmarker_num; i++ ) {
        if( wmarker_info[i].cf < 0.5 ) wmarker_info[i].id = -1;
    }

    j = 0;
    for( i = 0; i < wmarker_num; i++ ) {
        if( wmarker_info[i].id < 0 ) continue;
        sprev_info[LorR][j].marker = wmarker_info[i];
        sprev_info[LorR][j].count  = 1;
        j++;
    }
    sprev_num[LorR] = j;

    *marker_num  = wmarker_num;
    *marker_info = wmarker_info;

    return 0;
}

int arsDetectMarkerLite( ARUint8 *dataPtr, int thresh,
                         ARMarkerInfo **marker_info, int *marker_num, int LorR )
{
    ARInt16                *limage;
    int                    label_num;
    int                    *area, *clip, *label_ref;
    double                 *pos;
    int                    i;

    *marker_num = 0;

    limage = arsLabeling( dataPtr, thresh,
                          &label_num, &area, &pos, &clip, &label_ref, LorR );
    if( limage == 0 )    return -1;

    marker_info2 = arDetectMarker2( limage, label_num, label_ref,
                                    area, pos, clip, AR_AREA_MAX, AR_AREA_MIN,
                                    1.0, &wmarker_num);
    if( marker_info2 == 0 ) return -1;

    wmarker_info = arsGetMarkerInfo( dataPtr, marker_info2, &wmarker_num, LorR );
    if( wmarker_info == 0 ) return -1;

    for( i = 0; i < wmarker_num; i++ ) {
        if( wmarker_info[i].cf < 0.5 ) wmarker_info[i].id = -1;
    }


    *marker_num  = wmarker_num;
    *marker_info = wmarker_info;

    return 0;
}
