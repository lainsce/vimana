static YisVal cogito_video_view_new(void) {
  CogitoNode *n = cogito_node_new(COGITO_VIDEO_VIEW);
  n->kd->video_view.texture = NULL;
  n->kd->video_view.video_w = 0;
  n->kd->video_view.video_h = 0;
  n->kd->video_view.fit = 0; // cover
  return YV_OBJ(n);
}

static void cogito_video_view_set_fit(YisVal nv, YisVal fitv) {
  if (nv.tag != EVT_OBJ) return;
  CogitoNode *n = (CogitoNode *)nv.as.p;
  n->kd->video_view.fit = (uint8_t)fitv.as.i;
}
