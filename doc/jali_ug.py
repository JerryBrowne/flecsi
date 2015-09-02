#~----------------------------------------------------------------------------~#
# Copyright
#~----------------------------------------------------------------------------~#

#------------------------------------------------------------------------------#
# Configuration for User Guide
#------------------------------------------------------------------------------#

opts = {
   #---------------------------------------------------------------------------#
   # Document
   #
   # The Cinch document service supports multiple document targets
   # from within the distributed documentation tree, potentially within
   # a single file.  This option specifies which document should be used
   # to produce the output target of this configuration file.
   #---------------------------------------------------------------------------#

   'document' : 'User Guide',

   #---------------------------------------------------------------------------#
   # Chapters Prepend List
   #
   # This options allows you to specify an order for the first N chapters
   # of the document, potentially leaving the overall ordering arbitrary.
   #---------------------------------------------------------------------------#

   #'chapters-prepend' : [
   #],

   #---------------------------------------------------------------------------#
   # Chapters List
   #
   # This option allows you to specify an order for some or all of the
   # chapters in the the document.
   #---------------------------------------------------------------------------#

   'chapters' : [
      'Introduction'
   ]

   #---------------------------------------------------------------------------#
   # Chapters Append List
   #
   # This options allows you to specify an order for the last N chapters
   # of the document, potentially leaving the overall ordering arbitrary.
   #---------------------------------------------------------------------------#

   #'chapters-append' : [
   #]
}

#~---------------------------------------------------------------------------~-#
# Formatting options for vim.
# vim: set tabstop=4 shiftwidth=4 expandtab :
#~---------------------------------------------------------------------------~-#
