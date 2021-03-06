# ------------------
#     libglib2
# ------------------
# $Id: libglib2.sh 1902 2008-01-02 22:27:47Z Harry $
# Copyright (c) 2007, Ippei Ukai
# script skeleton Copyright (c) 2007, Ippei Ukai
# libglib2 specifics 2012, Harry van der Wolf


# prepare

# -------------------------------
# 20120411.0 HvdW Script tested
# 20121010.0 hvdw remove openmp stuff
# -------------------------------

# init

fail()
{
        echo "** Failed at $1 **"
        exit 1
}

ORGPATH=$PATH

VERSION="2.0"
FULLVERSION="2.0.0"

export LIBFFI_CFLAGS="-I$REPOSITORYDIR/libffi-3.0.13/include"
export LIBFFI_LIBS="-L$REPOSITORYDIR/lib -lffi"
export MSGFMT="$REPOSITORYDIR/bin/msgfmt"

let NUMARCH="0"

for i in $ARCHS
do
  NUMARCH=$(($NUMARCH + 1))
done

mkdir -p "$REPOSITORYDIR/bin";
mkdir -p "$REPOSITORYDIR/lib";
mkdir -p "$REPOSITORYDIR/include";


# compile

for ARCH in $ARCHS
do

 mkdir -p "$REPOSITORYDIR/arch/$ARCH/bin";
 mkdir -p "$REPOSITORYDIR/arch/$ARCH/lib";
 mkdir -p "$REPOSITORYDIR/arch/$ARCH/include";

 ARCHARGs=""
 MACSDKDIR=""

 if [ $ARCH = "i386" -o $ARCH = "i686" ] ; then
   TARGET=$i386TARGET
   MACSDKDIR=$i386MACSDKDIR
   ARCHARGs="$i386ONLYARG"
   OSVERSION="$i386OSVERSION"
   CC=$i386CC
   CXX=$i386CXX
#   myPATH=$ORGPATH
   ARCHFLAG="-m32"
 else [ $ARCH = "x86_64" ] ;
   TARGET=$x64TARGET
   MACSDKDIR=$x64MACSDKDIR
   ARCHARGs="$x64ONLYARG"
   OSVERSION="$x64OSVERSION"
   CC=$x64CC
   CXX=$x64CXX
   ARCHFLAG="-m64"
#   myPATH=/usr/local/bin:$PATH
 fi





 env \
  CC=$CC CXX=$CXX \
  CFLAGS="-isysroot $MACSDKDIR $ARCHFLAG $ARCHARGs $OTHERARGs -I$REPOSITORYDIR/include -O3 -dead_strip -fstrict-aliasing" \
  CXXFLAGS="-isysroot $MACSDKDIR $ARCHFLAG $ARCHARGs $OTHERARGs -I$REPOSITORYDIR/include -O3 -dead_strip -fstrict-aliasing" \
  CPPFLAGS="-I$REPOSITORYDIR -I$REPOSITORYDIR/arch/$ARCH -I$REPOSITORYDIR/arch/$ARCH/include -I$REPOSITORYDIR/include" \
  LDFLAGS="-L$REPOSITORYDIR/lib -mmacosx-version-min=$OSVERSION -L$MACSDKDIR/usr/lib -dead_strip -lresolv -bind_at_load $ARCHFLAG" \
  NEXT_ROOT="$MACSDKDIR" \
  ./configure --prefix="$REPOSITORYDIR" --disable-dependency-tracking \
  --host="$TARGET" --exec-prefix=$REPOSITORYDIR/arch/$ARCH \
  ZLIB_CFLAGS="-I$MACSDKDIR/usr/include" ZLIB_LIBS="-L$MACSDKDIR/usr/lib" \
  GETTEXT_CFLAGS="-I$REPOSITORYDIR/include" GETTEXT_LIBS="-L$REPOSITORYDIR/lib" \
  --disable-selinux --disable-fam --disable-xattr \
  --disable-gtk-doc --disable-gtk-doc-html --disable-gtk-doc-pdf \
  --disable-man --disable-dtrace --disable-systemtap \
  --enable-static --enable-shared  || fail "configure step of $ARCH";

 make clean
 make || fail "failed at make step of $ARCH"
 make $OTHERMAKEARGs install || fail "make install step of $ARCH"
done


# merge libglib2

for liba in lib/libglib-$VERSION.a lib/libglib-$FULLVERSION.dylib lib/libgmodule-$VERSION.a lib/libgmodule-$FULLVERSION.dylib lib/libgthread-$VERSION.a lib/libgthread-$FULLVERSION.dylib lib/libgobject-$VERSION.a lib/libgobject-$FULLVERSION.dylib lib/libgio-$VERSION.a lib/libgio-$FULLVERSION.dylib
do

 if [ $NUMARCH -eq 1 ] ; then
   if [ -f $REPOSITORYDIR/arch/$ARCHS/$liba ] ; then
	 echo "Moving arch/$ARCHS/$liba to $liba"
  	 mv "$REPOSITORYDIR/arch/$ARCHS/$liba" "$REPOSITORYDIR/$liba";
	   #Power programming: if filename ends in "a" then ...
	   [ ${liba##*.} = a ] && ranlib "$REPOSITORYDIR/$liba";
  	 continue
	 else
		 echo "Program arch/$ARCHS/$liba not found. Aborting build";
		 exit 1;
	 fi
 fi

 LIPOARGs=""

 
 for ARCH in $ARCHS
 do
	if [ -f $REPOSITORYDIR/arch/$ARCH/$liba ] ; then
		echo "Adding arch/$ARCH/$liba to bundle"
		LIPOARGs="$LIPOARGs $REPOSITORYDIR/arch/$ARCH/$liba"
	else
		echo "File arch/$ARCH/$liba was not found. Aborting build";
		exit 1;
	fi
 done


 lipo $LIPOARGs -create -output "$REPOSITORYDIR/$liba";
 #Power programming: if filename ends in "a" then ...
 [ ${liba##*.} = a ] && ranlib "$REPOSITORYDIR/$liba";

done

cp -rv "$REPOSITORYDIR/arch/$ARCH/lib/glib-2.0" "$REPOSITORYDIR/lib"

echo "First doing the install name stuff for glib libs"
# Do the name_change thing as the libs are linked to each other in the arc/$ARCH libs.
# We don't want that
for liba in lib/libglib-$VERSION.a lib/libglib-$FULLVERSION.dylib lib/libgmodule-$VERSION.a lib/libgmodule-$FULLVERSION.dylib lib/libgthread-$VERSION.a lib/libgthread-$FULLVERSION.dylib lib/libgobject-$VERSION.a lib/libgobject-$FULLVERSION.dylib lib/libgio-$VERSION.a lib/libgio-$FULLVERSION.dylib
do
   for lib in $(otool -L $REPOSITORYDIR/$liba | grep $REPOSITORYDIR/arch/$ARCH/lib | sed -e 's/ (.*$//' -e 's/^.*\///')
     do
        echo "Changing install name for: $lib inside : $liba for $ARCH"
        install_name_tool -change "$REPOSITORYDIR/arch/$ARCH/lib/$lib" "$REPOSITORYDIR/lib/$lib" $REPOSITORYDIR/$liba
     done
   install_name_tool -id "$REPOSITORYDIR/lib/$liba" $REPOSITORYDIR/$liba
done

if [ -f $REPOSITORYDIR/lib/libglib-$FULLVERSION.dylib ] ; then
         install_name_tool \
           -id "$REPOSITORYDIR/lib/libglib-$FULLVERSION.dylib" \
           "$REPOSITORYDIR/lib/libglib-$FULLVERSION.dylib";
           ln -sfn "libglib-$FULLVERSION.dylib" "$REPOSITORYDIR/lib/libglib-$VERSION.dylib";
           ln -sfn "libglib-$FULLVERSION.dylib" "$REPOSITORYDIR/lib/libglib-2.dylib";
           echo "install_name_tool id on libglib-$FULLVERSION.dylib plus symlinking";
fi

if [ -f $REPOSITORYDIR/lib/libgmodule-$FULLVERSION.dylib ] ; then
         install_name_tool \
           -id "$REPOSITORYDIR/lib/libgmodule-$FULLVERSION.dylib" \
           "$REPOSITORYDIR/lib/libgmodule-$FULLVERSION.dylib";
           ln -sfn "libgmodule-$FULLVERSION.dylib" "$REPOSITORYDIR/lib/libgmodule-$VERSION.dylib";
           ln -sfn "libgmodule-$FULLVERSION.dylib" "$REPOSITORYDIR/lib/libgmodule-2.dylib";
           echo "install_name_tool id on libgmodule-$FULLVERSION.dylib plus symlinking";
fi

if [ -f $REPOSITORYDIR/lib/libgthread-$FULLVERSION.dylib ] ; then
         install_name_tool \
           -id "$REPOSITORYDIR/lib/libgthread-$FULLVERSION.dylib" \
           "$REPOSITORYDIR/lib/libgthread-$FULLVERSION.dylib";
           ln -sfn "libgthread-$FULLVERSION.dylib" "$REPOSITORYDIR/lib/libgthread-$VERSION.dylib";
           ln -sfn "libgthread-$FULLVERSION.dylib" "$REPOSITORYDIR/lib/libgthread-2.dylib";
           echo "install_name_tool id on libgthread-$FULLVERSION.dylib plus symlinking";
fi

if [ -f $REPOSITORYDIR/lib/libgobject-$FULLVERSION.dylib ] ; then
         install_name_tool \
           -id "$REPOSITORYDIR/lib/libgobject-$FULLVERSION.dylib" \
           "$REPOSITORYDIR/lib/libgobject-$FULLVERSION.dylib";
           ln -sfn "libgobject-$FULLVERSION.dylib" "$REPOSITORYDIR/lib/libgobject-$VERSION.dylib";
           ln -sfn "libgobject-$FULLVERSION.dylib" "$REPOSITORYDIR/lib/libgobject-2.dylib";
           echo "install_name_tool id on libgobject-$FULLVERSION.dylib plus symlinking";
fi

if [ -f $REPOSITORYDIR/lib/libgio-$FULLVERSION.dylib ] ; then
         install_name_tool \
           -id "$REPOSITORYDIR/lib/libgio-$FULLVERSION.dylib" \
           "$REPOSITORYDIR/lib/libgio-$FULLVERSION.dylib";
           ln -sfn "libgio-$FULLVERSION.dylib" "$REPOSITORYDIR/lib/libgio-$VERSION.dylib";
           ln -sfn "libgio-$FULLVERSION.dylib" "$REPOSITORYDIR/lib/libgio-2.dylib";
           echo "install_name_tool id on libgio-$FULLVERSION.dylib plus symlinking";
fi


#pkgconfig
echo "Installing pkcconfig file glib-2.0.pc"
for ARCH in $ARCHS
do
 mkdir -p $REPOSITORYDIR/lib/pkgconfig
 sed 's/^exec_prefix.*$/exec_prefix=\$\{prefix\}/' $REPOSITORYDIR/arch/$ARCH/lib/pkgconfig/glib-2.0.pc > $REPOSITORYDIR/lib/pkgconfig/glib-2.0.pc
 break;
done

