//
// Created by liyinbin on 2022/3/6.
//

#include "flare/files/sequential_read_file.h"
#include "flare/log/logging.h"
#include "flare/base/errno.h"
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

namespace flare {

    sequential_read_file::~sequential_read_file() {
        if (_fd > 0) {
            ::close(_fd);
        }
    }

    flare_status sequential_read_file::open(const flare::file_path &path) noexcept {
        FLARE_CHECK(_fd == -1)<<"do not reopen";
        flare_status rs;
        _path = path;
        _fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC, 0644);
        if (_fd < 0) {
            FLARE_LOG(ERROR) << "open file: " << path << "error: " << errno << " " << strerror(errno);
            rs.set_error(errno, "%s", strerror(errno));
        }
        return rs;
    }

    flare_status sequential_read_file::read(size_t n, std::string *content) {
        flare::IOPortal portal;
        auto frs = read(n, &portal);
        if (frs.ok()) {
            auto size = portal.size();
            portal.cutn(content, size);
        }
        return frs;
    }

    flare_status sequential_read_file::read(size_t n, flare::cord_buf *buf) {
        ssize_t left = n;
        flare::IOPortal portal;
        flare_status frs;
        while (left > 0) {
            ssize_t read_len = portal.append_from_file_descriptor(_fd, static_cast<size_t>(left));
            if (read_len > 0) {
                left -= read_len;
            } else if (read_len == 0) {
                break;
            } else if (errno == EINTR) {
                continue;
            } else {
                FLARE_LOG(WARNING) << "read failed, err: " << flare_error()
                             << " fd: " << _fd << " size: " << n;
                frs.set_error(errno, "%s", flare_error());
                return frs;
            }
        }
        _has_read += portal.size();
        portal.swap(*buf);
        return frs;
    }

    flare_status sequential_read_file::skip(size_t n) {
        if (_fd > 0) {
            ::lseek(_fd, n, SEEK_CUR);
        }
        return flare_status();
    }

    bool sequential_read_file::is_eof(flare_status *frs) {
        std::error_code ec;
        auto size = flare::file_size(_path, ec);
        if(ec) {
            if(frs) {
                frs->set_error(errno, "%s", flare_error());
            }
            return false;
        }
        return _has_read == size;
    }

}  // namespace flare