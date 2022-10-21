Name:		myredispool	
Version:	0.0.1	
Release:	1%{?dist}
Summary:	A hiredispool rewriten with pure C++
License:	GPL
URL:		https://github.com/pathree/myredispool.git

BuildRequires:	gcc, gcc-c++, make
Requires:	redis >= 3.2.12

%description
Ref: https://github.com/aclisp/hiredispool
+ Rewrite the hiredispool with pure C++
+ Support unix path

%prep
echo "RPM_BUILD_ROOT = ${RPM_BUILD_ROOT}"
echo "RPM_BUILD_ROOT = ${buildroot}"


%build
cd src && make clean && make 

%install
rm -rf ${RPM_BUILD_ROOT}
mkdir -p ${RPM_BUILD_ROOT}/%{_bindir}
cp src/redis_test ${RPM_BUILD_ROOT}/%{_bindir}

%files
%{_bindir}/redis_test

#%doc

%clean
rm -rf ${RPM_BUILD_ROOT}

%changelog
* Fri Oct 21 2022 Path Ree <pathree@google.com>
- Initial package for version 0.0.1
