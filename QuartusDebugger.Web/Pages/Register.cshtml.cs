using System.ComponentModel.DataAnnotations;
using Microsoft.AspNetCore.Identity;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.RazorPages;
using Microsoft.AspNetCore.RateLimiting;

namespace QuartusDebugger.Web.Pages;

[EnableRateLimiting("accounts")]
public sealed class RegisterModel(UserManager<StudentAccount> users) : PageModel
{
    [BindProperty, Required, RegularExpression(@"^[0-9]{8}$", ErrorMessage = "Student ID must contain exactly eight digits.")]
    public string StudentId { get; set; } = "";
    [BindProperty, Required, StringLength(80)] public string FirstName { get; set; } = "";
    [BindProperty, Required, StringLength(80)] public string LastName { get; set; } = "";
    [BindProperty, Required, StringLength(128, MinimumLength = 10)] public string Password { get; set; } = "";
    [BindProperty, Required, Compare(nameof(Password), ErrorMessage = "Passwords must match.")]
    public string ConfirmPassword { get; set; } = "";
    public IActionResult OnGet() => User.Identity?.IsAuthenticated == true ? RedirectToPage("/Index") : Page();
    public async Task<IActionResult> OnPostAsync()
    {
        if (!ModelState.IsValid) return Page();
        var result = await users.CreateAsync(new StudentAccount
        { UserName = StudentId, FirstName = FirstName.Trim(), LastName = LastName.Trim() }, Password);
        if (!result.Succeeded)
        {
            foreach (var error in result.Errors) ModelState.AddModelError("", error.Code == "DuplicateUserName"
                ? "That student ID is already registered." : error.Description);
            return Page();
        }
        TempData["Message"] = "Account created. Sign in to continue.";
        return RedirectToPage("/Login");
    }
}
