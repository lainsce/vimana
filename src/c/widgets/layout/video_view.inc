    case COGITO_VIDEO_VIEW: {
      int vw = 0, vh = 0;
      vw = cogito_gst_video_width();
      vh = cogito_gst_video_height();
      if (vw > 0 && vh > 0) {
        n->kd->video_view.video_w = vw;
        n->kd->video_view.video_h = vh;
      }
      break;
    }
