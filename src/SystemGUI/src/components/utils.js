export const icon = {
  help: '<svg class="mr-2" style="width: 24px; height: 24px;" viewBox="0 0 24 24"><path fill="currentColor" d="M12 3C6.5 3 2 6.6 2 11c0 2.1 1 4.1 2.8 5.5 0 .6-.4 2.2-2.8 4.5 0 0 3.5 0 6.5-2.5 1.1.3 2.3.5 3.5.5 5.5 0 10-3.6 10-8s-4.5-8-10-8m1 12h-2v-2h2v2m1.8-5c-.3.4-.7.6-1.1.8-.3.2-.4.3-.5.5-.2.2-.2.4-.2.7h-2c0-.5.1-.8.3-1.1.2-.2.6-.5 1.1-.8.3-.1.5-.3.6-.5.1-.2.2-.5.2-.7 0-.3-.1-.5-.3-.7-.2-.2-.5-.3-.8-.3-.3 0-.5.1-.7.2-.2.1-.3.3-.3.6h-2c.1-.7.4-1.3.9-1.7.5-.4 1.2-.5 2.1-.5.9 0 1.7.2 2.2.6.5.4.8 1 .8 1.7.1.4 0 .8-.3 1.2z"></path></svg>',
  warning:
    '<svg class="mr-2" style="width: 24px; height: 24px;" viewBox="0 0 24 24"><path fill="currentColor" d="M13 14h-2V9h2m0 9h-2v-2h2M1 21h22L12 2 1 21z"></path></svg>',
  error:
    '<svg class="mr-2" style="width: 24px; height: 24px;" viewBox="0 0 24 24"><path fill="currentColor" d="M12 2c5.53 0 10 4.47 10 10s-4.47 10-10 10S2 17.53 2 12 6.47 2 12 2m3.59 5L12 10.59 8.41 7 7 8.41 10.59 12 7 15.59 8.41 17 12 13.41 15.59 17 17 15.59 13.41 12 17 8.41 15.59 7z"></path></svg>',
  primary:
    '<svg class="mr-2" style="width: 24px; height: 24px;" viewBox="0 0 24 24"><path fill="currentColor" d="M19 19H5V5h14m0-2H5a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2V5a2 2 0 0 0-2-2m-8 12h2v2h-2v-2m0-8h2v6h-2V7"></path></svg>',
  success:
    '<svg class="mr-2" style="width: 24px; height: 24px;" viewBox="0 0 24 24"><path fill="currentColor" d="M12 2C6.5 2 2 6.5 2 12s4.5 10 10 10 10-4.5 10-10S17.5 2 12 2m-2 15-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z"></path></svg>',
  tooltip:
    '<svg viewBox="0 0 24 24"><path fill="currentColor" d="M15.07,11.25L14.17,12.17C13.45,12.89 13,13.5 13,15H11V14.5C11,13.39 11.45,12.39 12.17,11.67L13.41,10.41C13.78,10.05 14,9.55 14,9C14,7.89 13.1,7 12,7A2,2 0 0,0 10,9H8A4,4 0 0,1 12,5A4,4 0 0,1 16,9C16,9.88 15.64,10.67 15.07,11.25M13,19H11V17H13M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12C22,6.47 17.5,2 12,2Z"></path></svg>',
};

export const portValidation = [
  {
    rule: "integer",
  },
  {
    rule: "minNumber",
    value: 1024,
  },
  {
    rule: "maxNumber",
    value: 65535,
  },
];

export const ipValidation = [
  {
    validator: (value) => {
      const ipv4Pattern =
        /^(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\.(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\.(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\.(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])$|^$/;
      return ipv4Pattern.test(value);
    },
    errorMessage: "Invalid IPv4 address",
  },
];

export const ssidValidation = [
  {
    rule: "minLength",
    value: 3,
  },
  {
    rule: "maxLength",
    value: 20,
  },
];

export const passwordValidation = [
  {
    rule: "minLength",
    value: 8,
  },
  {
    rule: "maxLength",
    value: 20,
  },
];
