Pod::Spec.new do |s|
  s.name     = 'Minizip'
  s.version  = '1.1.0.2017.03.20'
  s.license  = 'zlib'
  s.summary  = 'Minizip contrib in zlib with latest bug fixes that supports PKWARE disk spanning, AES encryption, and IO buffering'
  s.description = <<-DESC
Minizip zlib contribution that includes:
* AES encryption
* I/O buffering
* PKWARE disk spanning
It also has the latest bug fixes that having been found all over the internet including the minizip forum and zlib developer's mailing list.
DESC
  s.homepage = 'http://www.winimage.com/zLibDll/minizip.html'
  s.authors = 'Gilles Vollant', 'Nathan Moinvaziri'

  s.source   = { :git => 'https://github.com/nmoinvaz/minizip.git' }
  s.libraries = 'z'

  s.subspec 'Core' do |sp|
    sp.source_files = '{ioapi,ioapi_mem,ioapi_buf,unzip,zip,crypt}.{c,h}'
  end

  s.subspec 'AES' do |sp|
    sp.dependency 'Minizip/Core'
    sp.source_files = 'aes/*.{c,h}'
  end
end
