#!/usr/bin/ruby
#
# display.rb - sample script for redis-lcd-display.c
#
# how to use
#   $ sudo gem install redis
#   $ ruby display.rb
#
require 'redis'

r = Redis.new

count = 0

loop do
  t = Time.now
  t_str  = t.strftime("%Y/%m/%d %H")
  t_str += count % 2 == 0 ? ":" : " "
  t_str += t.strftime("%M")

  r.set "lcd:0", "hostname=" + `hostname -s`.chomp
  r.set "lcd:1", `LANG=C /sbin/ifconfig | grep -v 127.0.0.1 | grep 'inet ' | awk '{print $2}' | sed -e 's/addr://'`.chomp
  r.set "lcd:2", t_str
  
  count += 1
  sleep 1
end
