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
+ Rewrite the hiredispool with pure C++
+ Support unix path

%prep

%build
cd src && make clean && make 

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/%{_bindir}
cp src/redis_test %{buildroot}/%{_bindir}

%files
%{_bindir}/redis_test

#%doc

%clean
rm -rf %{buildroot}

%changelog
* Fri Oct 21 2022 Path Ree <pathree@google.com>
- Initial package for version %{_version}
