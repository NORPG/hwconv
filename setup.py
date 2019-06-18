from distutils.core import setup, Extension

module1 = Extension('hwconv',
                    sources = ['hwconv.c'])

setup (name = 'hwconv',
       version = '1.0',
       description = 'This is a demo package',
       ext_modules = [module1])
