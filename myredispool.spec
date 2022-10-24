Name:		myredispool	
Version:	%{_version}	
Release:	%{_release}%{?dist}
Summary:	A hiredispool rewriten with pure C++
License:	GPL
URL:		https://github.com/pathree/myredispool.git

BuildRequires:	gcc, gcc-c++, make
Requires:	redis >= 3.2.12

%description
Ref: https://github.com/aclisp/hiredispool
Ref: https://rpm-packaging-guide.github.io
Ref: https://docs.fedoraproject.org/en-US/packaging-guidelines/RPMMacros
+ Rewrite the hiredispool with pure C++
+ Support unix path

%prep

%build
make build

%install
make install DESTDIR=%{buildroot}

%files
/usr/local/lib/libmy_redis_pool.so
/usr/local/bin/redis_test

%doc

%clean
rm -rf %{buildroot}

%changelog
* Fri Oct 21 2022 Path Ree <pathree@google.com>
- Initial package for version %{_version}
