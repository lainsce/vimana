case COGITO_FAB_MENU: {
  // FAB Menu size metrics: S(0), M(1), L(2)
  // Size: S=56, M=80, L=96
  int size = n->kd->fab_menu.size;
  if (size == 0) {
    w = 56; h = 56;
  } else if (size == 2) {
    w = 96; h = 96;
  } else {
    w = 80; h = 80;
  }
  break;
}
