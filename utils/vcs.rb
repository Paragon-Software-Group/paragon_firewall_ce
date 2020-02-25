#!/usr/bin/env ruby
#
# Paragon Firewall Community Edition
# Copyright (C) 2019-2020  Paragon Software GmbH
#
# This file is part of Paragon Firewall Community Edition.
#
# Paragon Firewall Community Edition is free software: you can
# redistribute it and/or modify it under the terms of the GNU General
# Public License as published by the Free Software Foundation, either
# version 3 of the License, or (at your option) any later version.
#
# Paragon Firewall Community Edition is distributed in the hope that it
# will be useful, but WITHOUT ANY WARRANTY; without even the implied
# warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
# the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Paragon Firewall Community Edition. If not, see
#   <https://www.gnu.org/licenses/>.

require 'find'
require 'pathname'
require 'nokogiri'

module VCS
  extend self

  def spawn_get_stdout(*args)
    r, w = IO.pipe
    raise $? unless system(*args, {:out => w})
    w.close
    r.read
  end

  def find_svn_wcs
    results = []

    Find.find('.') do |path|
      path = Pathname.new(path)
      if path.basename.to_s == '.svn' && path.directory?
	results << path.dirname
	Find.prune
      end
    end

    results
  end

  def list_wc_files(wc_path)
    is_git_wc?(wc_path) ? list_git_wc_files(wc_path) : list_svn_wc_files(wc_path)
  end

  # List files in a SVN working copy
  def list_svn_wc_files(wc_path)
    files = []

    # Find all the files using `svn list`
    xml = spawn_get_stdout('xcrun', 'svn', 'list', '--xml', '--recursive', wc_path.to_s)
    doc = Nokogiri::XML(xml)

    doc.xpath('/lists/list').each do |list|
      list_path = Pathname.new(list['path'])
      files += list.xpath('./entry[@kind="file"]/name').map {|e| list_path + e.text}
    end

    # Filter out symlinks
    files.select!(&:file?)

    # Filter out deleted files, add 'added' files
    xml = spawn_get_stdout('xcrun', 'svn', 'status', '--xml', '--ignore-externals', wc_path.to_s)
    doc = Nokogiri::XML(xml)
    doc.xpath('/status/target').each do |target|
      target_path = Pathname.new(target['path'])

      target.xpath('./entry').each do |entry|
	entry_path = target_path + entry['path']

	case entry.xpath('./wc-status/@item').to_s
	when 'deleted' then files.delete(entry_path)
	when 'added'
	  files << entry_path if entry_path.file?
	end
      end
    end

    files
  end

  def list_git_wc_files(wc_path)
    # Find all the files using git ls-files
    list = spawn_get_stdout('git', '-C', wc_path.to_s, 'ls-files').lines.map(&:chomp)

    # Construct absolute paths
    list.map { |path| wc_path + path }
  end

  def is_git_wc?(wc_path)
    opts = { out: '/dev/null', err: '/dev/null' }
    system('git', '-C', wc_path.to_s, 'status', opts)
  end
end
