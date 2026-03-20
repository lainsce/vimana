// mpris_macos.mm — macOS Now Playing / Media Remote Command integration
//
// Uses MediaPlayer.framework (MPNowPlayingInfoCenter, MPRemoteCommandCenter)
// to display track metadata in Control Center and handle play/pause/next/prev.

#import <Foundation/Foundation.h>
#import <MediaPlayer/MediaPlayer.h>

static int cogito_mpris_native_pending = 0;
static bool cogito_mpris_native_ready = false;

extern "C" {

bool cogito_mpris_init_native(const char *app_name) {
  (void)app_name;
  if (cogito_mpris_native_ready) return true;

  MPRemoteCommandCenter *cc = [MPRemoteCommandCenter sharedCommandCenter];

  [cc.playCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
    (void)event;
    cogito_mpris_native_pending = 1; // play
    return MPRemoteCommandHandlerStatusSuccess;
  }];
  [cc.pauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
    (void)event;
    cogito_mpris_native_pending = 2; // pause
    return MPRemoteCommandHandlerStatusSuccess;
  }];
  [cc.togglePlayPauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
    (void)event;
    // Toggle: caller decides based on current state
    cogito_mpris_native_pending = 1; // treat as play (toggle)
    return MPRemoteCommandHandlerStatusSuccess;
  }];
  [cc.nextTrackCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
    (void)event;
    cogito_mpris_native_pending = 3; // next
    return MPRemoteCommandHandlerStatusSuccess;
  }];
  [cc.previousTrackCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
    (void)event;
    cogito_mpris_native_pending = 4; // prev
    return MPRemoteCommandHandlerStatusSuccess;
  }];
  [cc.stopCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
    (void)event;
    cogito_mpris_native_pending = 5; // stop
    return MPRemoteCommandHandlerStatusSuccess;
  }];

  cogito_mpris_native_ready = true;
  return true;
}

void cogito_mpris_shutdown_native(void) {
  if (!cogito_mpris_native_ready) return;
  MPRemoteCommandCenter *cc = [MPRemoteCommandCenter sharedCommandCenter];
  [cc.playCommand removeTarget:nil];
  [cc.pauseCommand removeTarget:nil];
  [cc.togglePlayPauseCommand removeTarget:nil];
  [cc.nextTrackCommand removeTarget:nil];
  [cc.previousTrackCommand removeTarget:nil];
  [cc.stopCommand removeTarget:nil];
  [[MPNowPlayingInfoCenter defaultCenter] setNowPlayingInfo:nil];
  cogito_mpris_native_ready = false;
  cogito_mpris_native_pending = 0;
}

void cogito_mpris_set_metadata_native(const char *title, const char *artist,
                                      const char *album, int64_t length_ms) {
  if (!cogito_mpris_native_ready) return;
  NSMutableDictionary *info = [NSMutableDictionary dictionary];
  if (title && title[0])
    info[MPMediaItemPropertyTitle] = [NSString stringWithUTF8String:title];
  if (artist && artist[0])
    info[MPMediaItemPropertyArtist] = [NSString stringWithUTF8String:artist];
  if (album && album[0])
    info[MPMediaItemPropertyAlbumTitle] = [NSString stringWithUTF8String:album];
  if (length_ms > 0)
    info[MPMediaItemPropertyPlaybackDuration] = @((double)length_ms / 1000.0);
  [[MPNowPlayingInfoCenter defaultCenter] setNowPlayingInfo:info];
}

void cogito_mpris_set_playback_status_native(int status) {
  if (!cogito_mpris_native_ready) return;
  MPNowPlayingInfoCenter *center = [MPNowPlayingInfoCenter defaultCenter];
  NSMutableDictionary *info = center.nowPlayingInfo
    ? [center.nowPlayingInfo mutableCopy]
    : [NSMutableDictionary dictionary];
  switch (status) {
    case 0: // stopped
      info[MPNowPlayingInfoPropertyPlaybackRate] = @(0.0);
      break;
    case 1: // playing
      info[MPNowPlayingInfoPropertyPlaybackRate] = @(1.0);
      break;
    case 2: // paused
      info[MPNowPlayingInfoPropertyPlaybackRate] = @(0.0);
      break;
  }
  [center setNowPlayingInfo:info];
}

int cogito_mpris_poll_native(void) {
  int a = cogito_mpris_native_pending;
  cogito_mpris_native_pending = 0;
  return a;
}

} // extern "C"
