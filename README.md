# nfcd

Source struture:

  android	JNI source files taken from Android 4.03 (https://android.googlesource.com/platform/packages/apps/Nfc.git
		and https://android.googlesource.com/platform/frameworks/base.git). The sources are patched to allow SE
		(Secure Element) handling from the SEEK for Android project (http://code.google.com/p/seek-for-android/)
		wrapper Wrapper files that expose static functions defined in the android directory

  jni		Simple layer emulating a JNI environment for the JNI source in android. jobject et al are patched in such
		a way that it acts as a smart pointer to the referenced object.

  src		Main implementation of nfcd. Socket code partially based on rilproxy. Nfc reading/writing based on Java
		code from Android (https://android.googlesource.com/platform/frameworks/base.git)
