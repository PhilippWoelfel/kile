#cmakedefine HAVE_OKULAR

#ifdef HAVE_OKULAR
	#define LIVEPREVIEW_AVAILABLE
#else
	#undef LIVEPREVIEW_AVAILABLE
#endif

#cmakedefine HAVE_POPPLER

#ifdef HAVE_POPPLER
	#define LIBPOPPLER_AVAILABLE
#else
	#undef LIBPOPPLER_AVAILABLE
#endif
